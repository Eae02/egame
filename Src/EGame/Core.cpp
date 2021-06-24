#include "Core.hpp"
#include "MainThreadInvoke.hpp"
#include "Platform/FontConfig.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"
#include "Graphics/RenderDoc.hpp"
#include "Graphics/Model.hpp"
#include "InputState.hpp"
#include "Event.hpp"
#include "Console.hpp"
#include "TranslationGizmo.hpp"
#include "RotationGizmo.hpp"
#include "GameController.hpp"
#include "Profiling/Profiler.hpp"
#include "Profiling/ProfilerPane.hpp"

#include <iostream>
#include <iomanip>
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
	static std::vector<std::pair<Profiler*, uint64_t>> pendingProfilers;
	static std::unique_ptr<ProfilerPane> profilerPane;
	
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
			auto [profiler, profilerFrame] = pendingProfilers.front();
			if (profilerFrame + MAX_CONCURRENT_FRAMES > FrameIdx())
				break;
			
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
		
		if (maxFrameTimeNS != 0)
		{
			uint64_t frameTimeNS = duration_cast<nanoseconds>(high_resolution_clock::now() - frameBeginTime).count();
			if (frameTimeNS < maxFrameTimeNS)
			{
				std::this_thread::sleep_for(std::chrono::nanoseconds(maxFrameTimeNS - frameTimeNS));
			}
		}
		
		frameCPUTimer.Stop();
		
		if (DevMode())
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
			maxFrameTimeNS = 1000000000ULL / (uint64_t)runConfig.framerateCap;
		}
		
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
		
		eg::DefineEventType<ResolutionChangedEvent>();
		eg::DefineEventType<ButtonEvent>();
		
		if (devMode)
		{
			console::Init();
		}
		
		int platformInitStatus = PlatformInit(runConfig);
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
			
			profilerPane = std::make_unique<ProfilerPane>();
			
			console::AddCommand("ppane", 0, [&] (std::span<const std::string_view> args, console::Writer& writer)
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
						writer.WriteLine(console::ErrorColor, "Invalid argument to ppane, should be 'show' or 'hide'");
						return;
					}
				}
				profilerPane->visible = visible;
			});
		}
		
		if (runConfig.initialize)
			runConfig.initialize();
		
		console::AddCommand("modelInfo", 1, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			const Model* model = eg::FindAsset<Model>(args[0]);
			if (model == nullptr)
			{
				writer.Write(console::ErrorColor, "The model ");
				writer.Write(console::ErrorColor.ScaleRGB(1.5f), args[0]);
				writer.WriteLine(console::ErrorColor, " doesn't exist");
				return;
			}
			
			writer.Write(console::InfoColor, "Information about ");
			writer.Write(console::InfoColorSpecial, args[0]);
			writer.WriteLine(console::InfoColor, ":");
			
			//Prepares column data
			size_t nameColLen = 0;
			size_t vertexColLen = 0;
			size_t triangleColLen = 0;
			std::vector<std::string> vertexStrings(model->NumMeshes());
			std::vector<std::string> triangleStrings(model->NumMeshes());
			for (size_t i = 0; i < model->NumMeshes(); i++)
			{
				vertexStrings[i] = std::to_string(model->GetMesh(i).numVertices);
				triangleStrings[i] = std::to_string(model->GetMesh(i).numIndices / 3);
				
				nameColLen = std::max(nameColLen, model->GetMesh(i).name.size());
				vertexColLen = std::max(vertexColLen, vertexStrings[i].size());
				triangleColLen = std::max(triangleColLen, triangleStrings[i].size());
			}
			
			//Writes information about meshes
			const char* meshAccessNames[] = { "gpu", "cpu", "gpu+cpu" };
			uint32_t totVertices = 0;
			uint32_t totIndices = 0;
			for (size_t i = 0; i < model->NumMeshes(); i++)
			{
				std::string str = "  mesh[" + std::to_string(i) + "] '";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, model->GetMesh(i).name);
				
				str = "'" + std::string(nameColLen + 1 - model->GetMesh(i).name.size(), ' ') + "V:";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, vertexStrings[i]);
				
				str = std::string(vertexColLen + 1 - vertexStrings[i].size(), ' ') + "T:";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, triangleStrings[i]);
				
				str = std::string(triangleColLen + 1 - triangleStrings[i].size(), ' ') + "A:";
				writer.Write(console::InfoColor, str);
				writer.WriteLine(console::InfoColorSpecial, meshAccessNames[(int)model->GetMesh(i).access]);
				
				totVertices += model->GetMesh(i).numVertices;
				totIndices += model->GetMesh(i).numIndices;
			}
			
			for (size_t i = 0; i < model->NumMaterials(); i++)
			{
				std::string str = "  mat[" + std::to_string(i) + "] '";
				writer.Write(console::InfoColor, str);
				writer.Write(console::InfoColorSpecial, model->GetMaterialName(i));
				writer.WriteLine(console::InfoColor, "'");
			}
			
			writer.Write(console::InfoColor, "  total vertices: ");
			std::string totalVerticesStr = std::to_string(totVertices);
			writer.WriteLine(console::InfoColorSpecial, totalVerticesStr);
			
			writer.Write(console::InfoColor, "  total triangles: ");
			std::string totalTrianglesStr = std::to_string(totIndices / 3);
			writer.WriteLine(console::InfoColorSpecial, totalTrianglesStr);
		});
		
		console::SetCompletionProvider("modelInfo", 0, [] (std::span<const std::string_view> args, eg::console::CompletionsList& list)
		{
			std::type_index typeIndex(typeid(Model));
			AssetCommandCompletionProvider(list, &typeIndex);
		});
		
		console::AddCommand("gmem", 0, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			if (gal::GetMemoryStat == nullptr)
			{
				writer.WriteLine(console::WarnColor, "gmem is not supported by this graphics API");
			}
			else
			{
				GraphicsMemoryStat memStat = gal::GetMemoryStat();
				
				std::ostringstream amountUsedStream;
				amountUsedStream << std::setprecision(2) << std::fixed << (memStat.allocatedBytes / (1024.0 * 1024.0));
				std::string amountUsedString = amountUsedStream.str();
				
				writer.Write(console::InfoColor, "Graphics memory info: ");
				writer.Write(console::InfoColorSpecial, amountUsedString);
				writer.Write(console::InfoColor, " MiB in use, ");
				writer.Write(console::InfoColorSpecial, std::to_string(memStat.numBlocks));
				writer.Write(console::InfoColor, " blocks, ");
				writer.Write(console::InfoColorSpecial, std::to_string(memStat.unusedRanges));
				writer.Write(console::InfoColor, " unused ranges");
			}
		});
		
		console::AddCommand("gpuinfo", 0, [&] (std::span<const std::string_view> args, console::Writer& writer)
		{
			writer.Write(console::InfoColor, "GPU Name:   ");
			writer.WriteLine(console::InfoColorSpecial, GetGraphicsDeviceInfo().deviceName);
			writer.Write(console::InfoColor, "GPU Vendor: ");
			writer.WriteLine(console::InfoColorSpecial, GetGraphicsDeviceInfo().deviceVendorName);
		});
		
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
		detail::shouldClose = false;
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
		TranslationGizmo::Destroy();
		RotationGizmo::Destroy();
		UnloadAssets();
		DestroyUploadBuffers();
		DestroyGraphicsAPI();
		DestroyPlatformFontConfig();
	}
}
