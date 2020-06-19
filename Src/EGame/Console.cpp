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

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#endif

namespace eg
{
	struct Command
	{
		std::string_view name;
		int minArgs;
		std::function<void(Span<const std::string_view>)> callback;
		std::vector<console::CompletionProviderCallback> completionProviders;
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
		
		std::vector<std::string_view> commandParts;
		
		int currentCompletion = 0;
		int completionRem = 0;
		std::vector<std::string> completions;
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
	
	static inline Command* FindCommandByName(std::string_view name)
	{
		for (Command& command : ctx->commands)
		{
			if (command.name == name)
				return &command;
		}
		return nullptr;
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
			ctx->commandParts.clear();
			IterateStringParts(ctx->textEdit.Text(), ' ', [&] (std::string_view part)
			{
				ctx->commandParts.push_back(part);
			});
			
			//Saves the current completion text so it can be restored later
			std::string currentCompletion;
			if ((int)ctx->completions.size() > ctx->currentCompletion)
				currentCompletion = ctx->completions[ctx->currentCompletion];
			
			//Updates command completion
			ctx->completions.clear();
			if (!ctx->textEdit.Text().empty() && !std::isspace(ctx->textEdit.Text().back()) &&
			    ctx->textEdit.CursorPos() == (int)ctx->textEdit.Text().size())
			{
				CompletionsList completionsList(ctx->commandParts.back(), ctx->completions);
				if (ctx->commandParts.size() == 1)
				{
					for (const eg::Command& cmd : ctx->commands)
					{
						completionsList.Add(cmd.name);
					}
				}
				else
				{
					Command* cmd = FindCommandByName(ctx->commandParts[0]);
					if (cmd != nullptr && cmd->completionProviders.size() >= ctx->commandParts.size() - 1)
					{
						CompletionProviderCallback& callback = cmd->completionProviders[ctx->commandParts.size() - 2];
						if (callback != nullptr)
						{
							callback(ctx->commandParts, completionsList);
						}
					}
				}
				
				std::sort(ctx->completions.begin(), ctx->completions.end());
			}
			
			//Sets the current completion index so that it is the same completion as before
			ctx->currentCompletion = 0;
			for (int i = 0; i < (int)ctx->completions.size(); i++)
			{
				if (ctx->completions[i] == currentCompletion)
				{
					ctx->currentCompletion = i;
					break;
				}
			}
			
			if (!ctx->completions.empty())
			{
				ctx->completionRem = currentCompletion.size() - ctx->commandParts.back().size();
				if (IsButtonDown(Button::DownArrow) && !WasButtonDown(Button::DownArrow))
				{
					ctx->currentCompletion = glm::min(ctx->currentCompletion + 1, (int)ctx->completions.size() - 1);
				}
				if (IsButtonDown(Button::UpArrow) && !WasButtonDown(Button::UpArrow))
				{
					ctx->currentCompletion = glm::max(ctx->currentCompletion - 1, 0);
				}
				if ((IsButtonDown(Button::Tab) && !WasButtonDown(Button::Tab)) || 
				    (IsButtonDown(Button::LeftAlt) && !WasButtonDown(Button::LeftAlt)))
				{
					std::string_view rem(&currentCompletion[ctx->commandParts.back().size()], ctx->completionRem);
					ctx->textEdit.InsertText(rem);
					ctx->completions.clear();
				}
			}
			
			if (IsButtonDown(Button::Enter) && !WasButtonDown(Button::Enter))
			{
				ctx->textEdit.Clear();
				
				if (!ctx->commandParts.empty())
				{
					Command* cmd = FindCommandByName(ctx->commandParts[0]);
					if (cmd == nullptr)
					{
						std::ostringstream messageStream;
						messageStream << "Unknown command " << ctx->commandParts[0] << "";
						Write(ErrorColor, messageStream.str());
					}
					else if ((int)ctx->commandParts.size() <= cmd->minArgs)
					{
						std::ostringstream messageStream;
						messageStream << ctx->commandParts[0] << " requires at least " << cmd->minArgs << "arguments";
						Write(ErrorColor, messageStream.str());
					}
					else
					{
						cmd->callback(Span<const std::string_view>(ctx->commandParts.data() + 1, ctx->commandParts.size() - 1));
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
		
		if (!ctx->completions.empty())
		{
			std::string_view currentCompletion(ctx->completions[ctx->currentCompletion]);
			std::string_view completionRem = currentCompletion.substr(ctx->commandParts.back().size());
			spriteBatch.DrawText(*ctx->textEdit.Font(), completionRem, glm::vec2(innerMinX + ctx->textEdit.TextWidth(), baseY + padding),
			    ColorLin(1, 1, 1, opacity * 0.5f));
		}
		
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
		
		if (!ctx->completions.empty())
		{
			int numLines = std::min<int>(ctx->completions.size(), 4);
			float lineStep = font.LineHeight() * 1.5f;
			float textOffsetY = font.LineHeight() * 0.4f;
			
			float complBoxW = 200;
			float complBoxH = numLines * lineStep;
			float complBoxX = innerMinX + ctx->textEdit.TextWidth();
			float complBoxY = baseY - complBoxH;
			
			float textX = complBoxX + padding;
			
			spriteBatch.PushScissor(complBoxX, complBoxY, complBoxW, complBoxH);
			spriteBatch.DrawRect(Rectangle(complBoxX, complBoxY, complBoxW, complBoxH), ColorLin(ColorSRGB(0.2f, 0.2f, 0.25f, opacity * 0.75f)));
			
			for (int i = 0; i < numLines; i++)
			{
				int realIdx = i;
				float y = baseY - (i + 1) * lineStep;
				
				if (realIdx == ctx->currentCompletion)
				{
					ColorLin backColor = ColorSRGB::FromHex(0x6ba4d5);
					backColor.a = opacity;
					spriteBatch.DrawRect(Rectangle(complBoxX, y, complBoxW, lineStep), backColor);
				}
				
				spriteBatch.DrawText(*ctx->textEdit.Font(), ctx->completions[realIdx], glm::vec2(textX, y + textOffsetY), ColorLin(1, 1, 1, opacity));
			}
			
			spriteBatch.PopScissor();
		}
	}
	
	void console::AddCommand(std::string_view name, int minArgs, CommandCallback callback)
	{
		if (ctx == nullptr)
			return;
		ctx->commands.push_back({name, minArgs, std::move(callback)});
	}
	
	void console::SetCompletionProvider(std::string_view command, int arg, CompletionProviderCallback callback)
	{
		for (eg::Command& cmd : ctx->commands)
		{
			if (cmd.name == command)
			{
				if (arg >= (int)cmd.completionProviders.size())
					cmd.completionProviders.resize(arg + 1);
				cmd.completionProviders[arg] = std::move(callback);
				return;
			}
		}
		
		eg::Log(eg::LogLevel::Error, "con", "Cannot set completion provider for unknown command '{0}'.", command);
	}
	
	void console::CompletionsList::Add(std::string_view completion)
	{
		if (m_prefix.size() > completion.size())
			return;
		
		bool eq = std::equal(m_prefix.begin(), m_prefix.end(), completion.begin(),
			[] (char a, char b) { return tolower(a) == tolower(b); });
		
		if (eq)
		{
			m_completions->emplace_back(completion);
		}
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
	
	void TweakCommandsCompletionProvider(Span<const std::string_view> prevWords, console::CompletionsList& list)
	{
		if (!eg::DevMode())
			return;
		for (TweakVar& var : tweakVars)
		{
			list.Add(var.name);
		}
	}
	
	template <typename T>
	static inline void PrintTweakValueSet(std::string_view name, const T& value)
	{
		std::ostringstream stream;
		stream << "Set " << name << " to " << value;
		std::string msg = stream.str();
		console::Write(console::InfoColor, msg);
	}
	
	template <typename T>
	static inline void PrintTweakValueGet(std::string_view name, const T& value)
	{
		std::ostringstream stream;
		stream << name << " = " << value;
		std::string msg = stream.str();
		console::Write(console::InfoColor, msg);
	}
	
	static inline TweakVar* FindTweakVar(std::string_view name)
	{
		if (DevMode())
		{
			for (size_t i = 0; i < numTweakVars; i++)
			{
				if (tweakVars[i].name == name)
				{
					return &tweakVars[i];
				}
			}
		}
		return nullptr;
	}
	
	static void RegisterTweakCommands()
	{
		console::AddCommand("set", 2, [] (Span<const std::string_view> args)
		{
			TweakVar* var = FindTweakVar(args[0]);
			if (var == nullptr)
			{
				std::string msg = Concat({ "Tweakable variable not found: '", args[0], "'." });
				console::Write(console::WarnColor, msg);
			}
			else if (var->type == TweakVarType::Float)
			{
				try
				{
					var->valueF = glm::clamp(std::stof(std::string(args[1])), var->minF, var->maxF);
					PrintTweakValueSet(var->name, var->valueF);
				}
				catch (...)
				{
					std::string msg = Concat({ "Cannot parse: '", args[1], "' as float." });
					console::Write(console::WarnColor, msg);
				}
			}
			else if (var->type == TweakVarType::Int)
			{
				try
				{
					var->valueI = glm::clamp(std::stoi(std::string(args[1])), var->minI, var->maxI);
					PrintTweakValueSet(var->name, var->valueI);
				}
				catch (...)
				{
					std::string msg = Concat({ "Cannot parse: '", args[1], "' as int." });
					console::Write(console::WarnColor, msg);
				}
			}
			else if (var->type == TweakVarType::String)
			{
				var->valueS = args[1];
				PrintTweakValueSet(var->name, var->valueS);
			}
		});
		console::SetCompletionProvider("set", 0, TweakCommandsCompletionProvider);
		
		console::AddCommand("getvar", 1, [] (Span<const std::string_view> args)
		{
			TweakVar* var = FindTweakVar(args[0]);
			if (var == nullptr)
			{
				std::string msg = Concat({ "Tweakable variable not found: '", args[0], "'." });
				console::Write(console::WarnColor, msg);
			}
			else if (var->type == TweakVarType::Float)
			{
				PrintTweakValueGet(var->name, var->valueF);
			}
			else if (var->type == TweakVarType::Int)
			{
				PrintTweakValueGet(var->name, var->valueI);
			}
			else if (var->type == TweakVarType::String)
			{
				PrintTweakValueGet(var->name, var->valueS);
			}
		});
		console::SetCompletionProvider("getvar", 0, TweakCommandsCompletionProvider);
		
		console::AddCommand("toggle", 1, [] (Span<const std::string_view> args)
		{
			TweakVar* var = FindTweakVar(args[0]);
			if (var == nullptr)
			{
				std::string msg = Concat({ "Tweakable variable not found: '", args[0], "'." });
				console::Write(console::WarnColor, msg);
			}
			else if (var->type == TweakVarType::Int)
			{
				var->valueI = var->valueI ? 0 : 1;
				PrintTweakValueSet(var->name, var->valueI);
			}
			else
			{
				console::Write(console::WarnColor, "Only integer variables can be toggled");
			}
		});
		console::SetCompletionProvider("toggle", 0, [] (Span<const std::string_view> prevWords, console::CompletionsList& list)
		{
			if (!eg::DevMode())
				return;
			for (TweakVar& var : tweakVars)
			{
				if (var.type == TweakVarType::Int && var.minI == 0 && var.maxI == 1)
					list.Add(var.name);
			}
		});
		
		console::AddCommand("lsvar", 0, [] (Span<const std::string_view> args)
		{
			if (numTweakVars == 0 || !eg::DevMode())
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
