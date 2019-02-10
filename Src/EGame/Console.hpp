#pragma once

#include <functional>
#include <mutex>

#include "Span.hpp"
#include "Utils.hpp"
#include "Alloc/LinearAllocator.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "TextEdit.hpp"
#include "API.hpp"

namespace eg::console
{
	using CommandCallback = std::function<void(Span<const std::string_view>)>;
	
	EG_API extern const ColorLin InfoColor;
	EG_API extern const ColorLin WarnColor;
	EG_API extern const ColorLin ErrorColor;
	
	EG_API void Init();
	EG_API void Destroy();
	
	EG_API void AddCommand(std::string_view name, int minArgs, CommandCallback callback);
	
	EG_API void Write(const ColorLin& color, std::string_view text);
	
	EG_API void Clear();
	
	EG_API bool IsShown();
	
	EG_API void Show();
	EG_API void Hide();
	
	void Update(float dt);
	void Draw(SpriteBatch& spriteBatch, int screenWidth, int screenHeight);
}
