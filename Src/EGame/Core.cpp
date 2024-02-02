#include "Core.hpp"
#include "Assets/AssetLoad.hpp"
#include "Assets/DefaultAssetGenerator.hpp"
#include "Assets/WebAssetDownload.hpp"
#include "Console.hpp"
#include "ConsoleCommands.hpp"
#include "Event.hpp"
#include "GameController.hpp"
#include "Gizmo/GizmoCommon.hpp"
#include "Gizmo/RotationGizmo.hpp"
#include "Gizmo/TranslationGizmo.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/FullscreenShader.hpp"
#include "Graphics/Model.hpp"
#include "Graphics/RenderDoc.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"
#include "InputState.hpp"
#include "MainThreadInvoke.hpp"
#include "Platform/FontConfig.hpp"
#include "Profiling/Profiler.hpp"
#include "Profiling/ProfilerPane.hpp"

#include <iomanip>
#include <iostream>
#include <list>

using namespace std::chrono;

namespace eg
{
std::mutex detail::mutexMTI;
LinearAllocator detail::allocMTI;
detail::MTIBase* detail::firstMTI;
detail::MTIBase* detail::lastMTI;
std::thread::id detail::mainThreadId;

bool detail::shouldClose;
std::string detail::gameName;
std::string_view detail::exeDirPath;
uint64_t detail::frameIndex;
std::vector<FullscreenDisplayMode> detail::fullscreenDisplayModes;
int64_t detail::nativeDisplayModeIndex = -1;
float detail::displayScaleFactor = 1.0f;

void (*detail::imguiBeginFrame)(float dt);
void (*detail::imguiEndFrame)();

static bool profilingEnabled;
static std::list<Profiler> profilers;
static std::vector<Profiler*> availProfilers;
static std::vector<std::pair<Profiler*, uint64_t>> pendingProfilers;

static float dt = 0;
static uint64_t maxFrameTimeNS = 0;

high_resolution_clock::time_point lastFrameBeginTime;

void detail::ButtonDownEvent(Button button, bool isRepeat)
{
	if (!isRepeat && button != Button::Unknown && !detail::currentIS->IsButtonDown(button))
		detail::currentIS->OnButtonDown(button);
	RaiseEvent<ButtonEvent>({ button, true, isRepeat });
}

void detail::ButtonUpEvent(Button button, bool isRepeat)
{
	if (!isRepeat && button != Button::Unknown && detail::currentIS->IsButtonDown(button))
		detail::currentIS->OnButtonUp(button);
	RaiseEvent<ButtonEvent>({ button, false, isRepeat });
}

bool EnableProfiling()
{
	if (profilingEnabled)
		return false;
	profilingEnabled = true;
	eg::Log(eg::LogLevel::Info, "p", "Profiling enabled");
	ProfilerPane::s_instance.reset(new ProfilerPane);
	return true;
}

void detail::RunFrame(IGame& game)
{
	auto frameBeginTime = high_resolution_clock::now();

	uint64_t deltaNS = duration_cast<nanoseconds>(frameBeginTime - lastFrameBeginTime).count();
	dt = static_cast<float>(deltaNS) / 1E9f;

	*detail::previousIS = *detail::currentIS;
	detail::currentIS->cursorDeltaX = 0;
	detail::currentIS->cursorDeltaY = 0;
	detail::inputtedText.clear();

	if (profilingEnabled)
	{
		if (availProfilers.empty())
		{
			Profiler::current = &profilers.emplace_back();
		}
		else
		{
			Profiler::current = availProfilers.back();
			availProfilers.pop_back();
		}

		Profiler::current->Reset();
	}

	auto frameCPUTimer = StartCPUTimer("Frame");

	detail::PlatformStartFrame();

	{
		auto cpuTimer = StartCPUTimer("GPU Sync");
		gal::BeginFrame();
	}

	auto gpuTimer = StartGPUTimer("Frame");

	while (!pendingProfilers.empty())
	{
		auto [profiler, profilerFrame] = pendingProfilers.front();
		if (profilerFrame + MAX_CONCURRENT_FRAMES > FrameIdx())
			break;

		std::optional<ProfilingResults> result = profiler->GetResults();
		if (!result.has_value())
			break;

		ProfilerPane::Instance()->AddFrameResult(std::move(*result));

		pendingProfilers.erase(pendingProfilers.begin());
		availProfilers.push_back(profiler);
	}

	int newDrawableW, newDrawableH;
	gal::GetDrawableSize(newDrawableW, newDrawableH);
	if (newDrawableW != detail::resolutionX || newDrawableH != detail::resolutionY)
	{
		detail::resolutionX = newDrawableW;
		detail::resolutionY = newDrawableH;
		game.ResolutionChanged(detail::resolutionX, detail::resolutionY);
		RaiseEvent(ResolutionChangedEvent{ detail::resolutionX, detail::resolutionY });
	}

	SpriteBatch::overlay.Reset();

	if (detail::imguiBeginFrame)
		detail::imguiBeginFrame(dt);

	game.RunFrame(dt);

	if (detail::imguiEndFrame)
		detail::imguiEndFrame();

	if (ProfilerPane::Instance())
	{
		ProfilerPane::Instance()->Draw(SpriteBatch::overlay, detail::resolutionX, detail::resolutionY);
	}

	console::Update(dt);
	console::Draw(SpriteBatch::overlay, detail::resolutionX, detail::resolutionY);

	// Processes main thread invokes
	for (detail::MTIBase* mti = detail::firstMTI; mti != nullptr; mti = mti->next)
		mti->Invoke();
	detail::firstMTI = detail::lastMTI = nullptr;
	detail::allocMTI.Reset();

	eg::RenderPassBeginInfo rpBeginInfo;
	rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
	rpBeginInfo.depthLoadOp = AttachmentLoadOp::Load;
	SpriteBatch::overlay.UploadAndRender(detail::resolutionX, detail::resolutionY, rpBeginInfo);

	gpuTimer.Stop();

	gal::EndFrame();

	if (maxFrameTimeNS != 0)
	{
		uint64_t frameTimeNS = duration_cast<nanoseconds>(high_resolution_clock::now() - frameBeginTime).count();
		if (frameTimeNS < maxFrameTimeNS)
		{
			std::this_thread::sleep_for(std::chrono::nanoseconds(maxFrameTimeNS - frameTimeNS));
		}
	}

	frameCPUTimer.Stop();

	if (Profiler::current != nullptr)
	{
		pendingProfilers.emplace_back(Profiler::current, FrameIdx());
	}

	detail::cFrameIdx = (detail::cFrameIdx + 1) % MAX_CONCURRENT_FRAMES;
	detail::frameIndex++;
	lastFrameBeginTime = frameBeginTime;
}

int detail::Run(const RunConfig& runConfig, std::unique_ptr<IGame> (*createGame)())
{
	if (runConfig.framerateCap != 0)
	{
		maxFrameTimeNS = 1000000000ULL / static_cast<uint64_t>(runConfig.framerateCap);
	}

	devMode = HasFlag(runConfig.flags, RunFlags::DevMode);
	detail::createAssetPackage = HasFlag(runConfig.flags, RunFlags::CreateAssetPackage);
	disableAssetPackageCompression = HasFlag(runConfig.flags, RunFlags::AssetPackageFast);

	if (const char* devEnv = getenv("EG_DEV"))
	{
		if (std::strcmp(devEnv, "true") == 0)
			devMode = true;
		else if (std::strcmp(devEnv, "false") == 0)
			devMode = false;
		else
		{
			Log(LogLevel::Warning, "misc",
			    R"(Could not parse EG_DEV environment variable, should be either "true" or "false".)");
		}
	}

	const char* createEapEnv = getenv("EG_CREATE_EAP");
	if (createEapEnv != nullptr && std::strcmp(createEapEnv, "true") == 0 && devMode)
	{
		detail::createAssetPackage = true;
	}

	if (runConfig.gameName != nullptr)
		detail::gameName = runConfig.gameName;
	else
		detail::gameName = "Untitled Game";

	eg::DefineEventType<ResolutionChangedEvent>();
	eg::DefineEventType<ButtonEvent>();
	eg::DefineEventType<RelativeMouseModeLostEvent>();

	if (devMode)
	{
		console::Init();
	}

	int platformInitStatus = detail::PlatformInit(runConfig);
	if (platformInitStatus != 0)
		return platformInitStatus;

	renderdoc::Init();
	InitPlatformFontConfig();
	RegisterDefaultAssetGenerator();
	LoadAssetGenLibrary();
	RegisterAssetLoaders();
	LoadGameControllers();

	gal::GetDeviceInfo(detail::graphicsDeviceInfo);

	SpriteBatch::InitStatic();
	TranslationGizmo::Initialize();
	RotationGizmo::Initialize();
	if (DevMode())
	{
		SpriteFont::LoadDevFont();
		EnableProfiling();
	}

	auto initialize = runConfig.initialize;

	detail::WebDownloadAssetPackages(
		[=]
		{
			if (initialize)
				initialize();

			RegisterConsoleCommands();

			for (CallbackNode* node = onInit; node != nullptr; node = node->next)
			{
				node->callback();
			}

			std::unique_ptr<IGame> game = createGame();

			detail::currentIS = new InputState;
			detail::previousIS = new InputState;

			detail::PruneDownloadedAssetPackages();

			gal::EndLoading();

			MarkUploadBuffersAvailable();

			resolutionX = -1;
			resolutionY = -1;
			detail::shouldClose = false;
			frameIndex = 0;

			lastFrameBeginTime = high_resolution_clock::now();

			PlatformRunGameLoop(std::move(game));
		});

	return 0;
}

void detail::CoreUninitialize()
{
	for (auto* node = detail::onShutdown; node != nullptr; node = node->next)
	{
		node->callback();
	}

	delete detail::currentIS;
	delete detail::previousIS;

	profilers.clear();
	console::Destroy();
	SpriteBatch::overlay = {};
	SpriteFont::UnloadDevFont();
	SpriteBatch::DestroyStatic();
	TranslationGizmo::Destroy();
	RotationGizmo::Destroy();
	if (detail::gizmoPipeline.handle)
		detail::gizmoPipeline.Destroy();
	detail::DestroyFullscreenShaders();
	UnloadAssets();
	DestroyUploadBuffers();
	DestroyGraphicsAPI();
	DestroyPlatformFontConfig();
}
} // namespace eg
