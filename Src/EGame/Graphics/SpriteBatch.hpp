#pragma once

#include "../Color.hpp"
#include "../Geometry/Rectangle.hpp"
#include "../Utils.hpp"
#include "AbstractionHL.hpp"

namespace eg
{
enum class SpriteFlags
{
	None = 0,
	FlipX = 1,
	FlipY = 2,
	RedToAlpha = 4,
	ForceLowestMipLevel = 8,
	FlipYIfOpenGL = 16
};

enum class SpriteBlend
{
	Alpha = 0,
	Additive = 1,
	Overwrite = 2
};

EG_BIT_FIELD(SpriteFlags)

enum class TextFlags
{
	None = 0,
	NoPixelAlign = 1,
	DropShadow = 2
};

EG_BIT_FIELD(TextFlags)

class EG_API SpriteBatch
{
public:
	SpriteBatch() = default;

	void PushScissor(int x, int y, int width, int height);
	void PushScissorF(float x, float y, float width, float height);
	void PushScissorRect(const Rectangle& rect) { PushScissorF(rect.x, rect.y, rect.w, rect.h); }

	void PopScissor() { m_scissorStack.pop_back(); }

	void PushBlendState(SpriteBlend blendState);
	void PopBlendState();

	/**
	 * Adds a sprite to the spritebatch.
	 * @param texture The texture to use for the sprite.
	 * @param position The position of the origin in input space.
	 * @param color Constant color which will be multiplied with the texture color.
	 * @param scale Scale factor.
	 * @param flags Sprite flags.
	 * @param rotation Angle of rotation, specified clockwise in radians.
	 * @param origin Sprite origin in texture space.
	 */
	void Draw(
		const Texture& texture, const glm::vec2& position, const ColorLin& color, float scale = 1,
		SpriteFlags flags = SpriteFlags::None, float rotation = 0, const glm::vec2& origin = {})
	{
		Draw(
			texture, position, color,
			Rectangle(0, 0, static_cast<float>(texture.Width()), static_cast<float>(texture.Height())), scale, flags,
			rotation, origin);
	}

	void Draw(
		const Texture& texture, const glm::vec2& position, const ColorLin& color, const Rectangle& texRectangle,
		float scale = 1, SpriteFlags flags = SpriteFlags::None, float rotation = 0, glm::vec2 origin = {});

	void Draw(
		const Texture& texture, const Rectangle& rectangle, const ColorLin& color,
		SpriteFlags flags = SpriteFlags::None);

	void Draw(
		const Texture& texture, const Rectangle& rectangle, const ColorLin& color, const Rectangle& texRectangle,
		SpriteFlags flags = SpriteFlags::None);

	void DrawTextMultiline(
		const class SpriteFont& font, std::string_view text, const glm::vec2& position, const ColorLin& color,
		float scale = 1, float lineSpacing = 0, glm::vec2* sizeOut = nullptr, TextFlags flags = TextFlags::None,
		const ColorLin* secondColor = nullptr);

	void DrawText(
		const class SpriteFont& font, std::string_view text, const glm::vec2& position, const ColorLin& color,
		float scale = 1, glm::vec2* sizeOut = nullptr, TextFlags flags = TextFlags::None,
		const ColorLin* secondColor = nullptr);

	void DrawRectBorder(const Rectangle& rectangle, const ColorLin& color, float width = 1);

	void DrawRect(const Rectangle& rectangle, const ColorLin& color);

	void DrawLine(const glm::vec2& begin, const glm::vec2& end, const ColorLin& color, float width = 1);

	void Reset();
	void Upload();
	void Render(int screenWidth, int screenHeight, const glm::mat3* matrix = nullptr) const;
	void UploadAndRender(
		int screenWidth, int screenHeight, const RenderPassBeginInfo& rpBeginInfo, const glm::mat3* matrix = nullptr);

	[[deprecated]] void Begin() { Reset(); }
	[[deprecated]] void End(int screenWidth, int screenHeight, const RenderPassBeginInfo& rpBeginInfo)
	{
		UploadAndRender(screenWidth, screenHeight, rpBeginInfo);
	}
	[[deprecated]] void End(
		int screenWidth, int screenHeight, const RenderPassBeginInfo& rpBeginInfo, const glm::mat3& matrix)
	{
		UploadAndRender(screenWidth, screenHeight, rpBeginInfo, &matrix);
	}

	bool Empty() const { return m_batches.empty(); }

	static void InitStatic();
	static void DestroyStatic();

	static SpriteBatch overlay;

	float opacityScale = 1;

private:
	void InitBatch(const Texture& texture, SpriteFlags flags);
	void AddQuadIndices();

	struct Vertex
	{
		glm::vec2 position;
		glm::vec2 texCoord;
		uint8_t color[4];

		Vertex(const glm::vec2& _position, const glm::vec2& _texCoord, const ColorLin& _color, float opacityScale);
	};

	std::vector<Vertex> m_vertices;
	std::vector<uint32_t> m_indices;

	struct ScissorRectangle
	{
		int x;
		int y;
		int width;
		int height;

		bool operator==(const ScissorRectangle& rhs) const
		{
			return x == rhs.x && y == rhs.y && width == rhs.width && height == rhs.height;
		}

		bool operator!=(const ScissorRectangle& rhs) const { return !(rhs == *this); }
	};

	struct Batch
	{
		TextureRef texture;
		bool redToAlpha;
		uint32_t mipLevel;
		uint32_t firstIndex;
		uint32_t numIndices;
		bool enableScissor;
		ScissorRectangle scissor;
		SpriteBlend blend;
	};

	std::vector<Batch> m_batches;

	std::vector<ScissorRectangle> m_scissorStack;
	std::vector<SpriteBlend> m_blendStateStack;

	float m_positionScale[2];

	uint32_t m_vertexBufferCapacity = 0;
	uint32_t m_indexBufferCapacity = 0;
	Buffer m_vertexBuffer;
	Buffer m_indexBuffer;

	bool m_canRender = false;
};
} // namespace eg
