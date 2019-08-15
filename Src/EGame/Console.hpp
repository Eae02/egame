#pragma once

#include <functional>
#include <mutex>
#include <typeindex>

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

namespace eg
{
	EG_API float* TweakVarFloat(std::string name, float value, float min = -INFINITY, float max = INFINITY) noexcept;
	EG_API int* TweakVarInt(std::string name, int value, int min = INT_MIN, int max = INT_MAX) noexcept;
	EG_API std::string* TweakVarStr(std::string name, std::string value) noexcept;
}
