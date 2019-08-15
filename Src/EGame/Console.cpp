#include "Console.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"

#ifdef _WIN32
#define DEFINE_CONSOLEV2_PROPERTIES
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#undef DrawText
#undef min
#undef max
#endif

namespace eg
{
	struct Command
	{
		std::string_view name;
		int minArgs;
		std::function<void(Span<const std::string_view>)> callback;
	};
	
	struct Line
	{
		ColorLin color;
		std::string_view text;
	};
	
	struct ConsoleContext
	{
		std::vector<Command> commands;
		
		bool shown = false;
		float showProgress = 0;
		
		float scroll = 0;
		float scrollTarget = 0;
		float maxScroll = 0;
		
		float scrollOpacity = 0;
		
		TextEdit textEdit;
		
		std::mutex linesMutex;
		std::vector<Line> lines;
		LinearAllocator allocator;
	};
	
	static ConsoleContext* ctx = nullptr;
	
	const ColorLin console::InfoColor = ColorLin(ColorSRGB::FromHex(0xD1E0E6));
	const ColorLin console::WarnColor = ColorLin(ColorSRGB::FromHex(0xF0B173));
	const ColorLin console::ErrorColor = ColorLin(ColorSRGB::FromHex(0xF55161));
	
	static void RegisterTweakCommands();
	
	void console::Init()
	{
		if (ctx != nullptr)
			return;
		ctx = new ConsoleContext;
		
		RegisterTweakCommands();
		
#ifdef _WIN32
		HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
		if (hOut != INVALID_HANDLE_VALUE)
		{
			DWORD dwMode = 0;
			if (GetConsoleMode(hOut, &dwMode))
			{
				dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
				SetConsoleMode(hOut, dwMode);
			}
		}
#endif
	}
	
	void console::Destroy()
	{
		delete ctx;
		ctx = nullptr;
	}
	
	void console::Write(const ColorLin& color, std::string_view text)
	{
		if (ctx == nullptr)
			return;
		
		std::lock_guard<std::mutex> lock(ctx->linesMutex);
		
		IterateStringParts(text, '\n', [&] (std::string_view line)
		{
			char* data = reinterpret_cast<char*>(ctx->allocator.Allocate(line.size(), 1));
			std::memcpy(data, line.data(), line.size());
			ctx->lines.push_back({color, std::string_view(data, line.size())});
			if (ctx->scroll > 1)
				ctx->scroll++;
		});
	}
	
	void console::Clear()
	{
		if (ctx == nullptr)
			return;
		
		std::lock_guard<std::mutex> lock(ctx->linesMutex);
		
		ctx->lines.clear();
		ctx->allocator.Reset();
		ctx->scroll = 0;
	}
	
	bool console::IsShown()
	{
		return ctx != nullptr && ctx->shown;
	}
	
	void console::Show()
	{
		if (ctx != nullptr)
			ctx->shown = true;
	}
	
	void console::Hide()
	{
		if (ctx != nullptr)
			ctx->shown = false;
	}
	
	void console::Update(float dt)
	{
		if (ctx == nullptr)
			return;
		if (ctx->textEdit.Font() == nullptr)
			ctx->textEdit.SetFont(&SpriteFont::DevFont());
		
		bool toggleShown = IsButtonDown(Button::Grave) && !WasButtonDown(Button::Grave);
		if (toggleShown && ctx->shown)
		{
			ctx->shown = false;
			toggleShown = false;
		}
		
		constexpr float TRANSITION_DURATION = 0.1f;
		
		const float d = dt / TRANSITION_DURATION;
		ctx->showProgress = glm::clamp(ctx->showProgress + (ctx->shown ? d : -d), 0.0f, 1.0f);
		
		ctx->scroll += std::min(dt * 10, 1.0f) * (ctx->scrollTarget - ctx->scroll);
		
		ctx->scrollOpacity = std::max(ctx->scrollOpacity - dt * 2, 0.0f);
		
		if (ctx->shown)
		{
			if (IsButtonDown(Button::Enter) && !WasButtonDown(Button::Enter))
			{
				std::vector<std::string_view> parts;
				IterateStringParts(ctx->textEdit.Text(), ' ', [&] (std::string_view part)
				{
					parts.push_back(part);
				});
				
				ctx->textEdit.Clear();
				
				if (!parts.empty())
				{
					auto it = std::find_if(ctx->commands.begin(), ctx->commands.end(), [&] (const Command& cmd)
					{
						return cmd.name == parts[0];
					});
					if (it == ctx->commands.end())
					{
						std::ostringstream messageStream;
						messageStream << "Unknown command " << parts[0] << "";
						Write(ErrorColor, messageStream.str());
					}
					else if ((int)parts.size() <= it->minArgs)
					{
						std::ostringstream messageStream;
						messageStream << parts[0] << " requires at least " << it->minArgs << "arguments";
						Write(ErrorColor, messageStream.str());
					}
					else
					{
						it->callback(Span<const std::string_view>(parts.data() + 1, parts.size() - 1));
					}
				}
			}
			
			if (ctx->maxScroll > 0)
			{
				ctx->scrollTarget += InputState::Current().scrollY - InputState::Previous().scrollY;
				if (ctx->scrollTarget > ctx->maxScroll)
					ctx->scrollTarget = ctx->maxScroll;
				if (ctx->scrollTarget < 0)
					ctx->scrollTarget = 0;
				ctx->scrollOpacity = 5;
			}
		}
		
		ctx->textEdit.Update(dt, ctx->shown);
		
		if (toggleShown && !ctx->shown)
		{
			ctx->shown = true;
		}
	}
	
	void console::Draw(SpriteBatch& spriteBatch, int screenWidth, int screenHeight)
	{
		if (ctx == nullptr || ctx->showProgress < 0.000001f)
			return;
		
		const int width = (int)(screenWidth * 0.8f);
		const int height = (int)(width * 0.2f);
		const int padding = (int)(width * 0.01f);
		const int baseX = (screenWidth - width) / 2;
		const int baseY = (int)(screenHeight - ctx->showProgress * height);
		const float opacity = ctx->showProgress * 0.75f;
		
		const int innerMinX = baseX + padding;
		const int innerMaxX = baseX + width - padding;
		
		const SpriteFont& font = SpriteFont::DevFont();
		
		spriteBatch.DrawRect(Rectangle(baseX, baseY, width, height), ColorLin(ColorSRGB(0.2f, 0.2f, 0.25f, opacity)));
		
		spriteBatch.PushScissor(innerMinX, baseY, width - padding * 2, font.Size() + padding * 2);
		
		ctx->textEdit.Draw(glm::vec2(innerMinX, baseY + padding), spriteBatch, ColorLin(1, 1, 1, opacity));
		
		spriteBatch.PopScissor();
		
		const int lineY = baseY + padding * 2 + font.Size();
		
		float viewWindowHeight = height - (lineY - baseY) - padding * 2;
		ctx->maxScroll = ctx->lines.size() - viewWindowHeight / ctx->textEdit.Font()->LineHeight();
		
		spriteBatch.DrawLine(glm::vec2(innerMinX, lineY), glm::vec2(innerMaxX, lineY), ColorLin(1, 1, 1, opacity), 0.5f);
		
		spriteBatch.PushScissor(innerMinX, lineY + 1, width - padding * 2, height - (lineY - baseY));
		
		{
			std::lock_guard<std::mutex> lock(ctx->linesMutex);
			float y = lineY + padding - ctx->textEdit.Font()->LineHeight() * ctx->scroll;
			for (int i = (int)ctx->lines.size() - 1; i >= 0; i--)
			{
				spriteBatch.DrawText(*ctx->textEdit.Font(), ctx->lines[i].text, glm::vec2(innerMinX, std::round(y)),
					ctx->lines[i].color);
				y += ctx->textEdit.Font()->LineHeight();
			}
		}
		
		if (ctx->maxScroll > 0)
		{
			const int SCROLL_WIDTH = 2;
			const float scrollHeight = viewWindowHeight * viewWindowHeight / (ctx->lines.size() * ctx->textEdit.Font()->LineHeight());
			const float scrollY = (viewWindowHeight - scrollHeight) * (ctx->scroll / ctx->maxScroll);
			const Rectangle rectangle(innerMaxX - SCROLL_WIDTH, lineY + padding + scrollY, SCROLL_WIDTH, scrollHeight);
			spriteBatch.DrawRect(rectangle, ColorLin(1, 1, 1, opacity * std::min(ctx->scrollOpacity, 1.0f)));
		}
		
		spriteBatch.PopScissor();
	}
	
	void console::AddCommand(std::string_view name, int minArgs, CommandCallback callback)
	{
		if (ctx == nullptr)
			return;
		ctx->commands.push_back({name, minArgs, std::move(callback)});
	}
	
	enum class TweakVarType
	{
		Float,
		Int,
		String
	};
	
	struct TweakVar
	{
		std::string name;
		TweakVarType type;
		
		float valueF;
		int valueI;
		std::string valueS;
		float minF;
		float maxF;
		int minI;
		int maxI;
	};
	
	static std::array<TweakVar, 1024> tweakVars;
	static size_t numTweakVars;
	
	inline TweakVar* AddTweakVar(std::string name, TweakVarType type) noexcept
	{
		if (numTweakVars == tweakVars.size())
			EG_PANIC("Too many tweak variables");
		TweakVar* var = &tweakVars[numTweakVars++];
		var->name = std::move(name);
		var->type = type;
		return var;
	}
	
	float* TweakVarFloat(std::string name, float value, float min, float max) noexcept
	{
		 TweakVar* var = AddTweakVar(std::move(name), TweakVarType::Float);
		 var->valueF = value;
		 var->minF = min;
		 var->maxF = max;
		 return &var->valueF;
	}
	
	int* TweakVarInt(std::string name, int value, int min, int max) noexcept
	{
		 TweakVar* var = AddTweakVar(std::move(name), TweakVarType::Int);
		 var->valueI = value;
		 var->minI = min;
		 var->maxI = max;
		 return &var->valueI;
	}
	
	std::string* TweakVarStr(std::string name, std::string value) noexcept
	{
		TweakVar* var = AddTweakVar(std::move(name), TweakVarType::Int);
		var->valueS = std::move(value);
		return &var->valueS;
	}
	
	static void RegisterTweakCommands()
	{
		console::AddCommand("set", 2, [] (Span<const std::string_view> args)
		{
			auto varsEnd = tweakVars.begin() + numTweakVars;
			auto it = std::find_if(tweakVars.begin(), varsEnd, [&] (const TweakVar& v) { return v.name == args[0]; });
			if (it == varsEnd)
			{
				std::string msg = Concat({ "Tweakable variable not found: '", args[0], "'." });
				console::Write(console::WarnColor, msg);
			}
			else if (it->type == TweakVarType::Float)
			{
				try
				{
					it->valueF = glm::clamp(std::stof(std::string(args[1])), it->minF, it->maxF);
				}
				catch (...)
				{
					std::string msg = Concat({ "Cannot parse: '", args[1], "' as float." });
					console::Write(console::WarnColor, msg);
				}
			}
			else if (it->type == TweakVarType::Int)
			{
				try
				{
					it->valueI = glm::clamp(std::stoi(std::string(args[1])), it->minI, it->maxI);
				}
				catch (...)
				{
					std::string msg = Concat({ "Cannot parse: '", args[1], "' as int." });
					console::Write(console::WarnColor, msg);
				}
			}
			else if (it->type == TweakVarType::String)
			{
				it->valueS = args[1];
			}
		});
		
		console::AddCommand("lsvar", 0, [] (Span<const std::string_view> args)
		{
			if (numTweakVars == 0)
			{
				console::Write(console::ErrorColor, "There are no tweakable variables");
				return;
			}
			
			static const char* typeNames[] = { "flt", "int", "str" };
			std::vector<std::string> lines(numTweakVars);
			for (size_t i = 0; i < numTweakVars; i++)
			{
				std::ostringstream stream;
				stream << "  " << tweakVars[i].name << " [" << typeNames[(int)tweakVars[i].type] << "]: ";
				if (tweakVars[i].type == TweakVarType::Float)
					stream << tweakVars[i].valueF;
				else if (tweakVars[i].type == TweakVarType::Int)
					stream << tweakVars[i].valueI;
				else if (tweakVars[i].type == TweakVarType::String)
					stream << tweakVars[i].valueS;
				lines[i] = stream.str();
			}
			std::sort(lines.begin(), lines.end());
			console::Write(console::InfoColor, "Tweakable variables:");
			for (const std::string& line : lines)
				console::Write(console::InfoColor, line);
		});
	}
}
