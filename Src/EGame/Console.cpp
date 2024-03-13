#include "Console.hpp"
#include "Assert.hpp"
#include "Core.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"
#include "String.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <variant>

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
	console::CommandCallback callback;
	std::vector<console::CompletionProviderCallback> completionProviders;
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
	std::vector<std::vector<console::LineSegment>> lines;
	LinearAllocator allocator;

	std::vector<std::string_view> commandParts;

	int currentCompletion = 0;
	int completionRem = 0;
	std::vector<std::string> completions;
};

static ConsoleContext* ctx = nullptr;

const ColorLin console::InfoColor = ColorLin(ColorSRGB::FromHex(0xdaeaf0));
const ColorLin console::InfoColorSpecial = ColorLin(ColorSRGB::FromHex(0xe6f6fc));
const ColorLin console::WarnColor = ColorLin(ColorSRGB::FromHex(0xf7ac66));
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

static inline console::LineSegment MakeLineSegment(const ColorLin& color, std::string_view text)
{
	if (text.empty())
		return {};
	console::LineSegment segment;
	segment.text = ctx->allocator.MakeStringCopy(text);
	segment.color = color;
	return segment;
}

void console::Write(const ColorLin& color, std::string_view text)
{
	if (ctx == nullptr)
		return;

	std::lock_guard<std::mutex> lock(ctx->linesMutex);

	IterateStringParts(
		text, '\n',
		[&](std::string_view line)
		{
			ctx->lines.emplace_back().push_back(MakeLineSegment(color, line));
			if (ctx->scroll > 1)
				ctx->scroll++;
		});
}

void console::Writer::Flush()
{
	while (!m_pendingLines.empty() && m_pendingLines.back().empty())
		m_pendingLines.pop_back();

	if (ctx == nullptr || m_pendingLines.empty())
		return;

	{
		std::lock_guard<std::mutex> lock(ctx->linesMutex);
		ctx->lines.insert(ctx->lines.end(), m_pendingLines.begin(), m_pendingLines.end());
		if (ctx->scroll > 1)
			ctx->scroll += static_cast<float>(m_pendingLines.size());
	}

	m_pendingLines.clear();
}

void console::Writer::Write(const ColorLin& color, std::string_view text)
{
	if (ctx == nullptr)
		return;

	while (!text.empty() && text[0] == '\n')
	{
		NewLine();
		text = text.substr(1);
	}

	if (text.empty())
		return;

	if (m_pendingLines.empty())
	{
		NewLine();
	}

	if (m_pendingLines.back().empty() && !m_linePrefixText.empty())
	{
		m_pendingLines.back().push_back(MakeLineSegment(color.ScaleAlpha(m_linePrefixAlphaScale), m_linePrefixText));
	}

	size_t firstNewLine = text.find('\n');
	m_pendingLines.back().push_back(MakeLineSegment(color, text.substr(0, firstNewLine)));

	if (firstNewLine != std::string_view::npos)
	{
		NewLine();
		Write(color, text.substr(firstNewLine + 1));
	}
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
		IterateStringParts(
			ctx->textEdit.Text(), ' ', [&](std::string_view part) { ctx->commandParts.push_back(part); });

		// Saves the current completion text so it can be restored later
		std::string currentCompletion;
		if (ToInt64(ctx->completions.size()) > ctx->currentCompletion)
			currentCompletion = ctx->completions[ctx->currentCompletion];

		// Updates command completion
		ctx->completions.clear();
		if (!ctx->textEdit.Text().empty() && !std::isspace(ctx->textEdit.Text().back()) &&
		    ctx->textEdit.CursorPos() == ToInt64(ctx->textEdit.Text().size()))
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

		// Sets the current completion index so that it is the same completion as before
		ctx->currentCompletion = 0;
		for (size_t i = 0; i < ctx->completions.size(); i++)
		{
			if (ctx->completions[i] == currentCompletion)
			{
				ctx->currentCompletion = ToInt(i);
				break;
			}
		}

		if (!ctx->completions.empty())
		{
			ctx->completionRem = ToInt(currentCompletion.size()) - ToInt(ctx->commandParts.back().size());
			if (IsButtonDown(Button::DownArrow) && !WasButtonDown(Button::DownArrow))
			{
				ctx->currentCompletion = glm::min(ctx->currentCompletion + 1, ToInt(ctx->completions.size()) - 1);
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
			if (!ctx->commandParts.empty())
			{
				Command* cmd = FindCommandByName(ctx->commandParts[0]);
				if (cmd == nullptr)
				{
					std::ostringstream messageStream;
					messageStream << "Unknown command " << ctx->commandParts[0] << "";
					Write(ErrorColor, messageStream.str());
				}
				else if (ToInt(ctx->commandParts.size()) <= cmd->minArgs)
				{
					std::ostringstream messageStream;
					messageStream << ctx->commandParts[0] << " requires at least " << cmd->minArgs << "arguments";
					Write(ErrorColor, messageStream.str());
				}
				else
				{
					std::span<const std::string_view> args(ctx->commandParts.data() + 1, ctx->commandParts.size() - 1);
					std::string linePrefix = Concat({ ctx->commandParts[0], " " });
					Writer writer(linePrefix, 0.75f);
					cmd->callback(args, writer);
				}
			}

			ctx->textEdit.Clear();
		}

		if (ctx->maxScroll > 0)
		{
			ctx->scrollTarget += static_cast<float>(InputState::Current().scrollY - InputState::Previous().scrollY);
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

	const SpriteFont& font = SpriteFont::DevFont();

	const float fontScale = 1.0f; // DisplayScaleFactor();
	ctx->textEdit.SetFontScale(fontScale);
	const float scaledFontSize = static_cast<float>(font.Size()) * fontScale;
	const float scaledLineHeight = static_cast<float>(font.LineHeight()) * fontScale;

	const float width = static_cast<float>(screenWidth) * 0.8f;
	const float height = width * 0.2f;
	const float padding = width * 0.01f;
	const float baseX = (static_cast<float>(screenWidth) - width) / 2.0f;
	const float baseY = static_cast<float>(screenHeight) - ctx->showProgress * height;
	const float opacity = ctx->showProgress * 0.9f;

	const float innerMinX = baseX + padding;
	const float innerMaxX = baseX + width - padding;

	spriteBatch.DrawRect(Rectangle(baseX, baseY, width, height), ColorLin(ColorSRGB(0.2f, 0.2f, 0.25f, opacity)));

	spriteBatch.PushScissorF(innerMinX, baseY, width - padding * 2, scaledFontSize + padding * 2);

	ctx->textEdit.Draw(glm::vec2(innerMinX, baseY + padding), spriteBatch, ColorLin(1, 1, 1, opacity));

	if (!ctx->completions.empty())
	{
		std::string_view currentCompletion(ctx->completions[ctx->currentCompletion]);
		std::string_view completionRem = currentCompletion.substr(ctx->commandParts.back().size());
		spriteBatch.DrawText(
			*ctx->textEdit.Font(), completionRem, glm::vec2(innerMinX + ctx->textEdit.TextWidth(), baseY + padding),
			ColorLin(1, 1, 1, opacity * 0.5f), fontScale, nullptr, TextFlags::NoPixelAlign);
	}

	spriteBatch.PopScissor();

	const float lineY = baseY + padding * 2.0f + scaledFontSize;

	float viewWindowHeight = height - (lineY - baseY) - padding * 2.0f;
	ctx->maxScroll = static_cast<float>(ctx->lines.size()) - viewWindowHeight / scaledLineHeight;

	spriteBatch.DrawLine(
		glm::vec2(innerMinX, lineY), glm::vec2(innerMaxX, lineY), ColorLin(1, 1, 1, opacity), 0.5f * fontScale);

	spriteBatch.PushScissorF(innerMinX, lineY + fontScale, width - padding * 2, height - (lineY - baseY));

	{
		std::lock_guard<std::mutex> lock(ctx->linesMutex);
		float y = lineY + padding - scaledLineHeight * ctx->scroll;
		for (int i = ToInt(ctx->lines.size()) - 1; i >= 0; i--)
		{
			glm::vec2 textPos(innerMinX, std::round(y));
			for (const LineSegment& segment : ctx->lines[i])
			{
				spriteBatch.DrawText(
					*ctx->textEdit.Font(), segment.text, textPos, segment.color, fontScale, nullptr,
					TextFlags::DropShadow | TextFlags::NoPixelAlign);
				textPos.x += ctx->textEdit.Font()->GetTextExtents(segment.text).x * fontScale;
			}
			y += scaledLineHeight;
		}
	}

	if (ctx->maxScroll > 0)
	{
		const float scrollBarWidth = 2 * fontScale;
		const float scrollBarHeight =
			viewWindowHeight * viewWindowHeight / (static_cast<float>(ctx->lines.size()) * scaledLineHeight);
		const float scrollY = (viewWindowHeight - scrollBarHeight) * (ctx->scroll / ctx->maxScroll);
		const Rectangle rectangle(
			innerMaxX - scrollBarWidth, lineY + padding + scrollY, scrollBarWidth, scrollBarHeight);
		spriteBatch.DrawRect(rectangle, ColorLin(1, 1, 1, opacity * std::min(ctx->scrollOpacity, 1.0f)));
	}

	spriteBatch.PopScissor();

	if (!ctx->completions.empty())
	{
		int numLines = ToInt(std::min<size_t>(ctx->completions.size(), 4));
		float lineStep = scaledLineHeight * 1.5f;
		float textOffsetY = scaledLineHeight * 0.4f;

		float complBoxW = 200 * fontScale;
		float complBoxH = static_cast<float>(numLines) * lineStep;
		float complBoxX = innerMinX + ctx->textEdit.TextWidth();
		float complBoxY = baseY - complBoxH;

		float textX = complBoxX + padding;

		spriteBatch.PushScissorF(complBoxX, complBoxY, complBoxW, complBoxH);
		spriteBatch.DrawRect(
			Rectangle(complBoxX, complBoxY, complBoxW, complBoxH),
			ColorLin(ColorSRGB(0.2f, 0.2f, 0.25f, opacity * 0.75f)));

		for (int i = 0; i < numLines; i++)
		{
			int realIdx = i;
			float y = baseY - static_cast<float>(i + 1) * lineStep;

			if (realIdx == ctx->currentCompletion)
			{
				ColorLin backColor = ColorSRGB::FromHex(0x6ba4d5);
				backColor.a = opacity;
				spriteBatch.DrawRect(Rectangle(complBoxX, y, complBoxW, lineStep), backColor);
			}

			spriteBatch.DrawText(
				*ctx->textEdit.Font(), ctx->completions[realIdx], glm::vec2(textX, y + textOffsetY),
				ColorLin(1, 1, 1, opacity), fontScale, nullptr, TextFlags::NoPixelAlign);
		}

		spriteBatch.PopScissor();
	}
}

void console::AddCommand(std::string_view name, int minArgs, CommandCallback callback)
{
	if (ctx == nullptr)
		return;
	ctx->commands.push_back({ name, minArgs, std::move(callback) });
}

void console::AddCommand(std::string_view name, int minArgs, console::CommandCallbackOld callback)
{
	AddCommand(
		name, minArgs, [callbackInner = std::move(callback)](std::span<const std::string_view> args, class Writer&)
		{ callbackInner(args); });
}

void console::SetCompletionProvider(std::string_view command, int arg, CompletionProviderCallback callback)
{
	for (eg::Command& cmd : ctx->commands)
	{
		if (cmd.name == command)
		{
			if (arg >= ToInt(cmd.completionProviders.size()))
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

	bool eq = std::equal(
		m_prefix.begin(), m_prefix.end(), completion.begin(), [](char a, char b) { return tolower(a) == tolower(b); });

	if (eq)
	{
		m_completions->emplace_back(completion);
	}
}

using TweakVarValue = std::variant<float, int, std::string>;

struct TweakVar
{
	std::string_view name;
	const char* typeName;
	TweakVarValue value;
	TweakVarValue initialValue;
	float minF = 0;
	float maxF = 0;
	int minI = 0;
	int maxI = 0;
};

static std::unordered_map<std::string_view, TweakVar>* tweakVars;

inline TweakVar* AddTweakVar(std::string_view name, TweakVarValue value) noexcept
{
	if (tweakVars == nullptr)
		tweakVars = new std::unordered_map<std::string_view, TweakVar>;

	auto emplaceRes = tweakVars->emplace(name, TweakVar());
	if (!emplaceRes.second)
	{
		EG_PANIC("Multiple tweakable variables share the name '" << name << "'.");
	}
	TweakVar& var = emplaceRes.first->second;
	var.name = name;
	var.initialValue = value;
	var.value = std::move(value);
	return &var;
}

float* TweakVarFloat(std::string_view name, float value, float min, float max) noexcept
{
	TweakVar* var = AddTweakVar(name, value);
	var->minF = min;
	var->maxF = max;
	var->typeName = "flt";
	return &std::get<float>(var->value);
}

int* TweakVarInt(std::string_view name, int value, int min, int max) noexcept
{
	TweakVar* var = AddTweakVar(name, value);
	var->minI = min;
	var->maxI = max;
	var->typeName = "int";
	return &std::get<int>(var->value);
}

std::string* TweakVarStr(std::string_view name, std::string value) noexcept
{
	TweakVar* var = AddTweakVar(name, std::move(value));
	var->typeName = "str";
	return &std::get<std::string>(var->value);
}

static void TweakCommandsCompletionProvider(std::span<const std::string_view> prevWords, console::CompletionsList& list)
{
	if (!tweakVars)
		return;
	for (const auto& var : *tweakVars)
		list.Add(var.first);
}

template <typename T>
static inline void PrintTweakValueSet(std::string_view name, const T& value, console::Writer& writer)
{
	writer.Write(console::InfoColor.ScaleAlpha(0.8f), "Set ");
	writer.Write(console::InfoColor, name);
	writer.Write(console::InfoColor.ScaleAlpha(0.8f), " to ");
	writer.Write(console::InfoColorSpecial, LogToString(value));
}

static inline TweakVar* FindTweakVarOrPrintError(std::string_view name)
{
	if (tweakVars)
	{
		auto it = tweakVars->find(name);
		if (it != tweakVars->end())
			return &it->second;
	}
	std::string msg = Concat({ "Tweakable variable not found: '", name, "'." });
	console::Write(console::WarnColor, msg);
	return nullptr;
}

static void RegisterTweakCommands()
{
	console::AddCommand(
		"set", 2,
		[](std::span<const std::string_view> args, console::Writer& writer)
		{
			TweakVar* var = FindTweakVarOrPrintError(args[0]);
			if (var == nullptr)
				return;
			if (float* valueF = std::get_if<float>(&var->value))
			{
				try
				{
					*valueF = glm::clamp(std::stof(std::string(args[1])), var->minF, var->maxF);
					PrintTweakValueSet(var->name, *valueF, writer);
				}
				catch (...)
				{
					writer.WriteLine(console::WarnColor, Concat({ "Cannot parse: '", args[1], "' as float." }));
				}
			}
			else if (int* valueI = std::get_if<int>(&var->value))
			{
				try
				{
					*valueI = glm::clamp(std::stoi(std::string(args[1])), var->minI, var->maxI);
					PrintTweakValueSet(var->name, *valueI, writer);
				}
				catch (...)
				{
					writer.WriteLine(console::WarnColor, Concat({ "Cannot parse: '", args[1], "' as int." }));
				}
			}
			else if (std::string* valueS = std::get_if<std::string>(&var->value))
			{
				*valueS = args[1];
				PrintTweakValueSet(var->name, *valueS, writer);
			}
		});
	console::SetCompletionProvider("set", 0, TweakCommandsCompletionProvider);

	console::AddCommand(
		"get", 1,
		[](std::span<const std::string_view> args, console::Writer& writer)
		{
			if (TweakVar* var = FindTweakVarOrPrintError(args[0]))
			{
				writer.Write(console::InfoColor, var->name);
				writer.Write(console::InfoColor.ScaleAlpha(0.8f), " = ");
				std::visit(
					[&](const auto& value) { writer.Write(console::InfoColorSpecial, LogToString(value)); },
					var->value);
				writer.NewLine();
			}
		});
	console::SetCompletionProvider("get", 0, TweakCommandsCompletionProvider);

	console::AddCommand(
		"setinit", 1,
		[](std::span<const std::string_view> args, console::Writer& writer)
		{
			if (TweakVar* var = FindTweakVarOrPrintError(args[0]))
			{
				if (var->value == var->initialValue)
				{
					writer.WriteLine(console::InfoColor, "Variable already has it's initial value");
				}
				else
				{
					var->value = var->initialValue;
					std::visit([&](const auto& value) { PrintTweakValueSet(var->name, value, writer); }, var->value);
				}
			}
		});
	console::SetCompletionProvider("setinit", 0, TweakCommandsCompletionProvider);

	console::AddCommand(
		"toggle", 1,
		[](std::span<const std::string_view> args, console::Writer& writer)
		{
			if (TweakVar* var = FindTweakVarOrPrintError(args[0]))
			{
				if (int* value = std::get_if<int>(&var->value))
				{
					*value = *value ? 0 : 1;
					PrintTweakValueSet(var->name, *value, writer);
				}
				else
				{
					writer.WriteLine(console::WarnColor, "Only integer variables can be toggled");
				}
			}
		});
	console::SetCompletionProvider(
		"toggle", 0,
		[](std::span<const std::string_view> prevWords, console::CompletionsList& list)
		{
			if (!tweakVars)
				return;
			for (const auto& var : *tweakVars)
			{
				if (std::holds_alternative<int>(var.second.value) && var.second.minI == 0 && var.second.maxI == 1)
				{
					list.Add(var.first);
				}
			}
		});

	console::AddCommand(
		"lsvar", 0,
		[](std::span<const std::string_view> args, console::Writer& writer)
		{
			if (tweakVars == nullptr || tweakVars->empty() || !eg::DevMode())
			{
				writer.WriteLine(console::ErrorColor, "There are no tweakable variables");
				return;
			}

			std::vector<const TweakVar*> variables;
			for (const auto& var : *tweakVars)
			{
				if (args.size() == 0 || var.first.find(args[0]) != std::string::npos)
				{
					variables.push_back(&var.second);
				}
			}
			if (variables.empty())
			{
				writer.WriteLine(console::ErrorColor, "No variables match the search criteria");
				return;
			}
			std::sort(
				variables.begin(), variables.end(),
				[](const TweakVar* a, const TweakVar* b) { return a->name < b->name; });

			writer.WriteLine(console::InfoColor, "Tweakable variables:");
			for (const TweakVar* var : variables)
			{
				writer.Write(console::InfoColor, " ");
				writer.Write(console::InfoColor.ScaleAlpha(0.8f), var->typeName);
				writer.Write(console::InfoColor, " ");
				writer.Write(console::InfoColor, var->name);
				writer.Write(console::InfoColor.ScaleAlpha(0.8f), ": ");

				auto printValueVisitor = [&](const auto& value)
				{ writer.Write(console::InfoColorSpecial, LogToString(value)); };
				std::visit(printValueVisitor, var->value);
				if (var->value != var->initialValue)
				{
					writer.Write(console::InfoColor.ScaleAlpha(0.8f), " (initially ");
					std::visit(printValueVisitor, var->initialValue);
					writer.Write(console::InfoColor.ScaleAlpha(0.8f), ")");
				}
				writer.NewLine();
			}
		});
}
} // namespace eg
