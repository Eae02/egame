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
	class EG_API CompletionsList
	{
	public:
		CompletionsList(std::string_view prefix, std::vector<std::string>& completions)
			: m_prefix(prefix), m_completions(&completions) { }
		
		void Add(std::string_view completion);
		
	private:
		std::string_view m_prefix;
		std::vector<std::string>* m_completions;
	};
	
	using CommandCallback = std::function<void(Span<const std::string_view>)>;
	using CompletionProviderCallback = std::function<void(Span<const std::string_view> prevWords, CompletionsList& list)>;
	
	EG_API extern const ColorLin InfoColor;
	EG_API extern const ColorLin WarnColor;
	EG_API extern const ColorLin ErrorColor;
	
	EG_API void Init();
	EG_API void Destroy();
	
	EG_API void AddCommand(std::string_view name, int minArgs, CommandCallback callback);
	
	EG_API void SetCompletionProvider(std::string_view command, int arg, CompletionProviderCallback callback);
	
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
