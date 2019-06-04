#include "Core.hpp"
#include "MainThreadInvoke.hpp"
#include "Platform/FontConfig.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"
#include "Graphics/RenderDoc.hpp"
#include "InputState.hpp"
#include "Event.hpp"
#include "Console.hpp"
#include "TranslationGizmo.hpp"
#include "GameController.hpp"
#include "Profiling/Profiler.hpp"
#include "Profiling/ProfilerPane.hpp"

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
	
	extern bool createAssetPackage;
	
	void LoadAssetGenLibrary();
	void RegisterAssetLoaders();
	void RegisterDefaultAssetGenerator();
	void LoadGameControllers();
	
	int PlatformInit(const RunConfig& runConfig);
	void PlatformStartFrame();
	void PlatformRunGameLoop(std::unique_ptr<IGame> game);
	
	static std::list<Profiler> profilers;
	static std::vector<Profiler*> availProfilers;
	static std::vector<Profiler*> pendingProfilers;
	static std::unique_ptr<ProfilerPane> profilerPane;
	
	bool shouldClose = false;
	
	static float dt = 0;
	
	high_resolution_clock::time_point lastFrameBeginTime;
	
	void RunFrame(IGame& game)
	{
		auto frameBeginTime = high_resolution_clock::now();
		
		uint64_t deltaNS = duration_cast<nanoseconds>(frameBeginTime - lastFrameBeginTime).count();
		dt = deltaNS / 1E9f;
		
		*detail::previousIS = *detail::currentIS;
		detail::currentIS->cursorDeltaX = 0;
		detail::currentIS->cursorDeltaY = 0;
		detail::inputtedText.clear();
		
		if (DevMode())
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
		
		PlatformStartFrame();
		
		{
			auto cpuTimer = StartCPUTimer("GPU Sync");
			gal::BeginFrame();
		}
		
		auto gpuTimer = StartGPUTimer("Frame");
		
		while (!pendingProfilers.empty())
		{
			Profiler* profiler = pendingProfilers.front();
			std::optional<ProfilingResults> result = profiler->GetResults();
			if (!result.has_value())
				break;
			
			profilerPane->AddFrameResult(std::move(*result));
			
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
			RaiseEvent(ResolutionChangedEvent { detail::resolutionX, detail::resolutionY });
		}
		
		SpriteBatch::overlay.Begin();
		
		game.RunFrame(dt);
		
		if (profilerPane)
		{
			profilerPane->Draw(SpriteBatch::overlay, detail::resolutionX, detail::resolutionY);
		}
		
		console::Update(dt);
		console::Draw(SpriteBatch::overlay, detail::resolutionX, detail::resolutionY);
		
		//Processes main thread invokes
		for (detail::MTIBase* mti = detail::firstMTI; mti != nullptr; mti = mti->next)
			mti->Invoke();
		detail::firstMTI = detail::lastMTI = nullptr;
		detail::allocMTI.Reset();
		
		eg::RenderPassBeginInfo rpBeginInfo;
		rpBeginInfo.colorAttachments[0].loadOp = AttachmentLoadOp::Load;
		rpBeginInfo.depthLoadOp = AttachmentLoadOp::Load;
		SpriteBatch::overlay.End(detail::resolutionX, detail::resolutionY, rpBeginInfo);
		
		gpuTimer.Stop();
		
		gal::EndFrame();
		
		frameCPUTimer.Stop();
		
		if (DevMode())
		{
			pendingProfilers.push_back(Profiler::current);
		}
		
		detail::cFrameIdx = (detail::cFrameIdx + 1) % MAX_CONCURRENT_FRAMES;
		detail::frameIndex++;
		lastFrameBeginTime = frameBeginTime;
	}
	
	int detail::Run(const RunConfig& runConfig, std::unique_ptr<IGame> (*createGame)())
	{
		devMode = HasFlag(runConfig.flags, RunFlags::DevMode);
		createAssetPackage = HasFlag(runConfig.flags, RunFlags::CreateAssetPackage);
		
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
			createAssetPackage = true;
		}
		
		if (runConfig.gameName != nullptr)
			detail::gameName = runConfig.gameName;
		else
			detail::gameName = "Untitled Game";
		
		int platformInitStatus = PlatformInit(runConfig);
		if (platformInitStatus != 0)
			return platformInitStatus;
		
		eg::DefineEventType<ResolutionChangedEvent>();
		eg::DefineEventType<ButtonEvent>();
		
		if (devMode)
		{
			console::Init();
		}
		
		renderdoc::Init();
		InitPlatformFontConfig();
		RegisterDefaultAssetGenerator();
		LoadAssetGenLibrary();
		RegisterAssetLoaders();
		LoadGameControllers();
		
		gal::GetDeviceInfo(detail::graphicsDeviceInfo);
		
		SpriteBatch::InitStatic();
		TranslationGizmo::InitStatic();
		if (DevMode())
		{
			SpriteFont::LoadDevFont();
			
			profilers.resize(MAX_CONCURRENT_FRAMES + 1);
			profilerPane = std::make_unique<ProfilerPane>();
			
			console::AddCommand("ppane", 0, [&] (Span<const std::string_view> args)
			{
				bool visible = !profilerPane->visible;
				if (args.size() == 1)
				{
					if (args[0] == "show")
						visible = true;
					else if (args[0] == "hide")
						visible = false;
					else
					{
						console::Write(console::ErrorColor, "Invalid argument to ppane, should be 'show' or 'hide'");
						return;
					}
				}
				profilerPane->visible = visible;
			});
		}
		
		if (runConfig.initialize)
			runConfig.initialize();
		
		for (CallbackNode* node = onInit; node != nullptr; node = node->next)
		{
			node->callback();
		}
		
		std::unique_ptr<IGame> game = createGame();
		
		detail::currentIS = new InputState;
		detail::previousIS = new InputState;
		
		gal::EndLoading();
		
		MarkUploadBuffersAvailable();
		
		resolutionX = -1;
		resolutionY = -1;
		shouldClose = false;
		frameIndex = 0;
		
		lastFrameBeginTime = high_resolution_clock::now();
		
		PlatformRunGameLoop(std::move(game));
		
		return 0;
	}
	
	void CoreUninitialize()
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
		TranslationGizmo::DestroyStatic();
		UnloadAssets();
		DestroyUploadBuffers();
		DestroyGraphicsAPI();
		DestroyPlatformFontConfig();
	}
}
