#ifdef EG_WEB
#include "Core.hpp"
#include "Graphics/AbstractionHL.hpp"
#include "Graphics/SpriteFont.hpp"
#include "InputState.hpp"
#include "Event.hpp"

#include <emscripten.h>

namespace eg
{
	int PlatformInit(const RunConfig& runConfig)
	{
		GraphicsAPIInitArguments apiInitArguments;
		apiInitArguments.window = nullptr;
		apiInitArguments.enableVSync = HasFlag(runConfig.flags, RunFlags::VSync);
		apiInitArguments.defaultFramebufferSRGB = HasFlag(runConfig.flags, RunFlags::DefaultFramebufferSRGB);
		
		if (!InitializeGraphicsAPI(eg::GraphicsAPI::OpenGL, apiInitArguments))
		{
			return 1;
		}
		
		return 0;
	}
	
	extern bool shouldClose;
	
	void CoreUninitialize();
	void RunFrame(IGame& game);
	
	static std::unique_ptr<IGame> game;
	
	void PlatformRunGameLoop(std::unique_ptr<IGame> _game)
	{
		game = std::move(_game);
		
		emscripten_set_main_loop([]
		{
			if (!gal::IsLoadingComplete() || !SpriteFont::IsDevFontLoaded())
				return;
			RunFrame(*game);
		}, 0, 0);
	}
	
	void PlatformStartFrame()
	{
		
	}
	
	std::string GetClipboardText()
	{
		return "";
	}
	
	void SetClipboardText(const char* text)
	{
		
	}
}

#endif
