#pragma once

#include "API.hpp"
#include "Graphics/Format.hpp"
#include "Graphics/Graphics.hpp"
#include "InputState.hpp"
#include "String.hpp"
#include "Utils.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eg
{
class IGame
{
public:
	virtual ~IGame() = default;

	virtual void RunFrame(float dt) = 0;
	virtual void ResolutionChanged(int newWidth, int newHeight) {}
};

enum class RunFlags
{
	None = 0,
	DevMode = 1,
	CreateAssetPackage = 2,
	DefaultFramebufferSRGB = 4,
	VSync = 8,
	ForceDepthZeroToOne = 16,
	PreferIntegratedGPU = 32,
	PreferGLESPath = 64,
	AssetPackageFast = 128,
};

EG_BIT_FIELD(RunFlags)

struct FullscreenDisplayMode
{
	uint32_t resolutionX;
	uint32_t resolutionY;
	uint32_t refreshRate;

	bool operator==(const FullscreenDisplayMode& rhs) const
	{
		return resolutionX == rhs.resolutionX && resolutionY == rhs.resolutionY && refreshRate == rhs.refreshRate;
	}
	bool operator!=(const FullscreenDisplayMode& rhs) const { return !(rhs == *this); }
};

struct RunConfig
{
	const char* gameName = nullptr;
	GraphicsAPI graphicsAPI = GraphicsAPI::Preferred;
	std::string preferredGPUName;
	std::function<void()> initialize;
	RunFlags flags = RunFlags::None;
	Format defaultDepthStencilFormat = Format::Depth16;
	uint32_t framerateCap = 400;
	const FullscreenDisplayMode* fullscreenDisplayMode = nullptr;
	uint32_t minWindowW = 0;
	uint32_t minWindowH = 0;
};

namespace detail
{
EG_API int Run(const RunConfig& runConfig, std::unique_ptr<IGame> (*createGame)());

EG_API extern bool shouldClose;
EG_API extern std::string gameName;
EG_API extern std::string_view exeDirPath;
EG_API extern uint64_t frameIndex;
EG_API extern std::vector<FullscreenDisplayMode> fullscreenDisplayModes;
EG_API extern int64_t nativeDisplayModeIndex;
EG_API extern float displayScaleFactor;

EG_API extern void (*imguiBeginFrame)(float dt);
EG_API extern void (*imguiEndFrame)();

void ButtonDownEvent(Button button, bool isRepeat);
void ButtonUpEvent(Button button, bool isRepeat);

int PlatformInit(const RunConfig& runConfig, bool headless, std::function<void()> initCompleteCallback);

void PlatformStartFrame();
void PlatformRunGameLoop(std::unique_ptr<IGame> game);

void RunFrame(IGame& game);
void CoreUninitialize();
} // namespace detail

inline float DisplayScaleFactor()
{
	return detail::displayScaleFactor;
}

inline uint64_t FrameIdx()
{
	return detail::frameIndex;
}

inline std::span<const FullscreenDisplayMode> FullscreenDisplayModes()
{
	return detail::fullscreenDisplayModes;
}

inline int64_t NativeDisplayModeIndex()
{
	return detail::nativeDisplayModeIndex;
}

EG_API void SetDisplayModeFullscreen(const FullscreenDisplayMode& displayMode);
EG_API void SetDisplayModeFullscreenDesktop();
EG_API void SetDisplayModeWindowed();

EG_API bool VulkanAppearsSupported();

/**
 * Enables profiling if not already enabled.
 * @return false if profiling was already enabled prioir to this call.
 */
EG_API bool EnableProfiling();

/**
 * Runs a game. This is the main entry point of the library and will block until the game is closed.
 * The library will construct an instance of the supplied game class and invoke callbacks on that instance.
 * @tparam Game The type of the game class, should inherit from eg::IGame.
 * @param runConfig Configuration options for the library.
 * @return An integer value which can be returned from main.
 * 0 if the game ran successfully, or another value if an error occurred.
 */
template <typename Game, class = std::enable_if_t<std::is_base_of_v<IGame, Game>>>
int Run(const RunConfig& runConfig = {})
{
	return detail::Run(runConfig, []() -> std::unique_ptr<IGame> { return std::make_unique<Game>(); });
}

EG_API int InitializeHeadless(const RunConfig& runConfig);

/**
 * Gets the path to the directory where the executable is located, guaranteed to end with a directory separator.
 */
inline std::string_view ExeDirPath()
{
	return detail::exeDirPath;
}

/**
 * Constructs an absolute path from a path which is relative to the directory where the executable is located.
 * @param path A path which is relative to the executable.
 * @return An absolute version of the supplied path.
 */
inline std::string ExeRelPath(std::string_view path)
{
	return Concat({ detail::exeDirPath, path });
}

/**
 * @return The gameName string passed in the run configuration of eg::Run.
 */
inline const std::string& GameName()
{
	return detail::gameName;
}

/**
 * Stops the game.
 */
inline void Close()
{
	detail::shouldClose = true;
}

/**
 * Returns the current clipboard text.
 */
EG_API std::string GetClipboardText();

/**
 * Sets the current cliboard text.
 * @param text The text to save to the clipboard.
 */
EG_API void SetClipboardText(const char* text);

/**
 * Sets the game's window icon.
 * @param width The icon's width in pixels.
 * @param width The icon's height in pixels.
 * @param rgbaData Pointer to RGBA pixel data, must be width * height * 4 bytes long.
 * This memory can be freed after this call completes.
 */
EG_API void SetWindowIcon(uint32_t width, uint32_t height, const void* rgbaData);
} // namespace eg
