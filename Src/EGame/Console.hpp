#pragma once

#include <functional>
#include <mutex>
#include <typeindex>
#include <span>

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
	
	using CommandCallbackOld = std::function<void(std::span<const std::string_view>)>;
	using CommandCallback = std::function<void(std::span<const std::string_view>, class Writer&)>;
	using CompletionProviderCallback = std::function<void(std::span<const std::string_view> prevWords, CompletionsList& list)>;
	
	EG_API extern const ColorLin InfoColor;
	EG_API extern const ColorLin InfoColorSpecial;
	EG_API extern const ColorLin WarnColor;
	EG_API extern const ColorLin ErrorColor;
	
	EG_API void Init();
	EG_API void Destroy();
	
	EG_API void AddCommand(std::string_view name, int minArgs, CommandCallback callback);
	[[deprecated]] EG_API void AddCommand(std::string_view name, int minArgs, CommandCallbackOld callback);
	
	EG_API void SetCompletionProvider(std::string_view command, int arg, CompletionProviderCallback callback);
	
	EG_API void Write(const ColorLin& color, std::string_view text);
	
	struct LineSegment
	{
		ColorLin color;
		std::string_view text;
	};
	
	class EG_API Writer
	{
	public:
		explicit Writer(std::string_view linePrefixText = { }, float linePrefixAlphaScale = 1.0f)
			: m_linePrefixText(linePrefixText), m_linePrefixAlphaScale(linePrefixAlphaScale) { }
		
		~Writer()
		{
			Flush();
		}
		
		void Write(const ColorLin& color, std::string_view text);
		
		void WriteLine(const ColorLin& color, std::string_view text)
		{
			Write(color, text);
			NewLine();
		}
		
		void NewLine()
		{
			m_pendingLines.emplace_back();
		}
		
		void Flush();
		
	private:
		std::string_view m_linePrefixText;
		float m_linePrefixAlphaScale;
		std::vector<std::vector<LineSegment>> m_pendingLines;
	};
	
	EG_API void Clear();
	
	EG_API bool IsShown();
	
	EG_API void Show();
	EG_API void Hide();
	
	void Update(float dt);
	void Draw(SpriteBatch& spriteBatch, int screenWidth, int screenHeight);
}

namespace eg
{
	//Adds tweakable variables, name must have static lifetime!
	EG_API float* TweakVarFloat(std::string_view name, float value, float min = -INFINITY, float max = INFINITY) noexcept;
	EG_API int* TweakVarInt(std::string_view name, int value, int min = INT_MIN, int max = INT_MAX) noexcept;
	EG_API std::string* TweakVarStr(std::string_view name, std::string value) noexcept;
}
