#pragma once

#include "API.hpp"
#include "Graphics/Graphics.hpp"

namespace eg
{
	class IGame
	{
	public:
		virtual ~IGame() = default;
		
		virtual void Update() = 0;
		virtual void Draw() = 0;
	};
	
	struct RunConfig
	{
		const char* gameName = nullptr;
		GraphicsAPI graphicsAPI = GraphicsAPI::Preferred;
		bool debug = true;
	};
	
	namespace detail
	{
		EG_API int Run(const RunConfig& runConfig, std::unique_ptr<IGame> (*createGame)());
		
		extern EG_API bool shouldClose;
		extern EG_API std::string gameName;
	}
	
	inline const std::string& GameName()
	{
		return detail::gameName;
	}
	
	inline void Close()
	{
		detail::shouldClose = true;
	}
	
	template <typename Game>
	int Run(const RunConfig& runConfig = { })
	{
		return detail::Run(runConfig, [] () -> std::unique_ptr<IGame> { return std::make_unique<Game>(); });
	}
	
	EG_API std::string GetClipboardText();
	EG_API void SetClipboardText(const char* text);
}
