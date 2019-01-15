#pragma once

#include "API.hpp"
#include "Graphics/Graphics.hpp"

namespace eg
{
	class IGame
	{
	public:
		virtual ~IGame() = default;
		
		virtual void RunFrame(float dt) = 0;
	};
	
	struct RunConfig
	{
		const char* gameName = nullptr;
		GraphicsAPI graphicsAPI = GraphicsAPI::Preferred;
		void (*initialize)() = nullptr;
		bool debug = true;
		bool createAssetPackage = false;
	};
	
	namespace detail
	{
		EG_API int Run(const RunConfig& runConfig, std::unique_ptr<IGame> (*createGame)());
		
		extern EG_API bool shouldClose;
		extern EG_API std::string gameName;
		extern EG_API std::string_view exeDirPath;
		extern EG_API uint64_t frameIndex;
	}
	
	inline uint64_t FrameIdx()
	{
		return detail::frameIndex;
	}
	
	/**
	 * Runs a game. This is the main entry point of the library and will block until the game is closed.
	 * The library will construct an instance of the supplied game class and invoke callbacks on that instance.
	 * @tparam Game The type of the game class, should inherit from eg::IGame.
	 * @param runConfig Configuration options for the library.
	 * @return An integer value which can be returned from main.
	 * 0 if the game ran successfully, or another value if an error occurred.
	 */
	template <typename Game, class = std::enable_if_t<std::is_base_of_v<IGame, Game>>>
	int Run(const RunConfig& runConfig = { })
	{
		return detail::Run(runConfig, [] () -> std::unique_ptr<IGame> { return std::make_unique<Game>(); });
	}
	
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
}
