#include "Console.hpp"
#include "Graphics/SpriteBatch.hpp"
#include "Graphics/SpriteFont.hpp"

namespace eg::console
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
	
	const ColorLin InfoColor = ColorLin(ColorSRGB::FromHex(0xD1E0E6));
	const ColorLin WarnColor = ColorLin(ColorSRGB::FromHex(0xF0B173));
	const ColorLin ErrorColor = ColorLin(ColorSRGB::FromHex(0xF55161));
	
	void Init()
	{
		if (ctx != nullptr)
			return;
		ctx = new ConsoleContext;
	}
	
	void Destroy()
	{
		delete ctx;
		ctx = nullptr;
	}
	
	void Write(const ColorLin& color, std::string_view text)
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
	
	void Clear()
	{
		if (ctx == nullptr)
			return;
		
		std::lock_guard<std::mutex> lock(ctx->linesMutex);
		
		ctx->lines.clear();
		ctx->allocator.Reset();
		ctx->scroll = 0;
	}
	
	bool IsShown()
	{
		return ctx != nullptr && ctx->shown;
	}
	
	void Show()
	{
		if (ctx != nullptr)
			ctx->shown = true;
	}
	
	void Hide()
	{
		if (ctx != nullptr)
			ctx->shown = false;
	}
	
	void Update(float dt)
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
						it->callback(Span<const std::string_view>(&parts[1], parts.size() - 1));
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
	
	void Draw(SpriteBatch& spriteBatch, int screenWidth, int screenHeight)
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
		
		spriteBatch.DrawRect(Rectangle(baseX, baseY, width, height), ColorLin(0.2f, 0.2f, 0.25f, opacity));
		
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
	
	void AddCommand(std::string_view name, int minArgs, CommandCallback callback)
	{
		if (ctx == nullptr)
			return;
		ctx->commands.push_back({name, minArgs, std::move(callback)});
	}
}
