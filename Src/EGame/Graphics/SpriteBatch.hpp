#pragma once

#include "AbstractionHL.hpp"
#include "../Color.hpp"
#include "../Rectangle.hpp"
#include "../Utils.hpp"

#include <stack>

namespace eg
{
	enum class FlipFlags
	{
		Normal = 0,
		FlipX = 1,
		FlipY = 2
	};
	
	EG_BIT_FIELD(FlipFlags)
	
	class EG_API SpriteBatch
	{
	public:
		SpriteBatch();
		
		void Begin();
		
		void PushScissor(int x, int y, int width, int height);
		
		void PopScissor()
		{
			m_scissorStack.pop();
		}
		
		/**
		 * Adds a sprite to the spritebatch.
		 * @param texture The texture to use for the sprite.
		 * @param position The position of the origin in input space.
		 * @param color Constant color which will be multiplied with the texture color.
		 * @param scale Scale factor.
		 * @param flipFlags Controlls sprite texture flipping.
		 * @param rotation Angle of rotation, specified clockwise in radians.
		 * @param origin Sprite origin in texture space.
		 */
		void Draw(const Texture& texture, const glm::vec2& position, const ColorLin& color, float scale = 1,
			FlipFlags flipFlags = FlipFlags::Normal, float rotation = 0, const glm::vec2& origin = { })
		{
			Draw(texture, position, color, Rectangle(0, 0, texture.Width(), texture.Height()), scale,
				flipFlags, rotation, origin);
		}
		
		void Draw(const Texture& texture, const glm::vec2& position, const ColorLin& color,
			const Rectangle& texRectangle, float scale = 1, FlipFlags flipFlags = FlipFlags::Normal,
			float rotation = 0, glm::vec2 origin = { });
		
		void Draw(const Texture& texture, const Rectangle& rectangle, const ColorLin& color, FlipFlags flipFlags)
		{
			Draw(texture, rectangle, color, Rectangle(0, 0, texture.Width(), texture.Height()), flipFlags);
		}
		
		void Draw(const Texture& texture, const Rectangle& rectangle, const ColorLin& color,
			const Rectangle& texRectangle, FlipFlags flipFlags);
		
		void DrawTextMultiline(const class SpriteFont& font, std::string_view text, const glm::vec2& position,
			const ColorLin& color, float size = 16, glm::vec2* sizeOut = nullptr);
		
		void DrawText(const class SpriteFont& font, std::string_view text, const glm::vec2& position,
			const ColorLin& color, float scale = 1, glm::vec2* sizeOut = nullptr);
		
		void DrawRectBorder(const Rectangle& rectangle, const ColorLin& color, float width = 1);
		
		void DrawRect(const Rectangle& rectangle, const ColorLin& color);
		
		void DrawLine(const glm::vec2& begin, const glm::vec2& end, const ColorLin& color, float width = 1);
		
		void End(int screenWidth, int screenHeight, const RenderPassBeginInfo& rpBeginInfo);
		
		void End(int screenWidth, int screenHeight, const RenderPassBeginInfo& rpBeginInfo, const glm::mat3& matrix);
		
		bool Empty() const
		{
			return m_batches.empty();
		}
		
		static void InitStatic();
		static void DestroyStatic();
		
		static SpriteBatch overlay;
		
	private:
		void InitBatch(const Texture& texture);
		void AddQuadIndices();
		
		struct Vertex
		{
			glm::vec2 position;
			glm::vec2 texCoord;
			uint8_t color[4];
			
			Vertex(const glm::vec2& _position, const glm::vec2& _texCoord, const ColorLin& _color);
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
			
			bool operator!=(const ScissorRectangle& rhs) const
			{
				return !(rhs == *this);
			}
		};
		
		struct Batch
		{
			TextureRef texture;
			uint32_t firstIndex;
			uint32_t numIndices;
			bool enableScissor;
			ScissorRectangle scissor;
		};
		
		std::vector<Batch> m_batches;
		
		std::stack<ScissorRectangle> m_scissorStack;
		
		float m_positionScale[2];
		
		uint32_t m_vertexBufferCapacity = 0;
		uint32_t m_indexBufferCapacity = 0;
		Buffer m_vertexBuffer;
		Buffer m_indexBuffer;
	};
}
