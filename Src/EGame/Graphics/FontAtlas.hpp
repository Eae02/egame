#pragma once

#include "../API.hpp"
#include "../Utils.hpp"

#include <span>
#include <optional>
#include <vector>

namespace eg
{
	struct Character
	{
		uint32_t id;
		uint16_t textureX;
		uint16_t textureY;
		uint16_t width;
		uint16_t height;
		int32_t xOffset;
		int32_t yOffset;
		float xAdvance;
	};
	
	struct KerningPair
	{
		uint32_t first;
		uint32_t second;
		int32_t amount;
	};
	
	struct EG_API GlyphRange
	{
		static const GlyphRange ASCII;
		static const GlyphRange LatinSupplement;
		static const GlyphRange LatinExtended;
		
		uint32_t start;
		uint32_t end;
		
		GlyphRange(uint32_t _start, uint32_t _end)
			: start(_start), end(_end) { }
		
		bool operator<(const GlyphRange& other) const
		{
			return start < other.start;
		}
		
		bool operator>(const GlyphRange& other) const
		{
			return start > other.start;
		}
		
		bool operator==(const GlyphRange& other) const
		{
			return start == other.start && end == other.end;
		}
		
		bool operator!=(const GlyphRange& other) const
		{
			return !operator==(other);
		}
	};
	
	class EG_API FontAtlas
	{
	public:
		FontAtlas() = default;
		
		/**
		 * Creates an atlas by rendering a font file. Any format supported by FreeType can be rendered.
		 * @param fontPath The path to the font file.
		 * @param size The font size to render at.
		 * @param glyphRanges A list of glyph ranges to include in the atlas,
		 * must be in sorted in ascending order of start and not have any overlap.
		 * @param atlasWidth Hint for the width of the output atlas.
		 * @param atlasHeight Hint for the height of the output atlas.
		 * @return The rendered font atlas, or none if an error occurred.
		 */
		static std::optional<FontAtlas> Render(const std::string& fontPath, uint32_t size,
			std::span<const GlyphRange> glyphRanges, int atlasWidth = -1, int atlasHeight = -1);
		
		/**
		 * Creates an atlas by rendering a font file stored in memory.
		 * Any format supported by FreeType can be rendered.
		 * @param data The font file contents in memory.
		 * @param size The font size to render at.
		 * @param glyphRanges A list of glyph ranges to include in the atlas,
		 * must be in sorted in ascending order of start and not have any overlap.
		 * @param atlasWidth Hint for the width of the output atlas.
		 * @param atlasHeight Hint for the height of the output atlas.
		 * @return The rendered font atlas, or none if an error occurred.
		 */
		static std::optional<FontAtlas> Render(std::span<const char> data, uint32_t size,
			std::span<const GlyphRange> glyphRanges, int atlasWidth = -1, int atlasHeight = -1);
		
		/**
		 * Creates a font atlas from an FNT file.
		 * @param path The path to the FNT file.
		 * @return The font atlas, or none if an error occurred.
		 */
		static std::optional<FontAtlas> FromFNT(const std::string& path);
		
		static std::optional<FontAtlas> FromFNTMemory(std::span<const char> fntData, std::span<const char> imgData);
		
		void Serialize(std::ostream& stream) const;
		static FontAtlas Deserialize(std::istream& stream);
		
		float LineHeight() const
		{
			return m_lineHeight;
		}
		
		int Size() const
		{
			return m_size;
		}
		
		float SpaceAdvance() const
		{
			return m_spaceAdvance;
		}
		
		uint32_t AtlasWidth() const
		{
			return m_atlasData.width;
		}
		
		uint32_t AtlasHeight() const
		{
			return m_atlasData.height;
		}
		
		const uint8_t* AtlasData() const
		{
			return m_atlasData.data;
		}
		
		const Character* GetCharacter(uint32_t c) const;
		
		const Character& GetCharacterOrDefault(uint32_t c) const;
		
		int GetKerning(uint32_t first, uint32_t second) const;
		
		glm::vec2 GetTextExtents(std::string_view text) const;
		
		std::string WordWrap(std::string_view text, float maxWidth) const;
		
	protected:
		void FreeAtlasData()
		{
			std::free(m_atlasData.data);
			m_atlasData.data = nullptr;
		}
		
	private:
		template <typename ReadLineCB, typename LoadImageCB>
		static std::optional<FontAtlas> FromFNTInternal(ReadLineCB readLineCB, LoadImageCB loadImage, std::string_view name);
		
		static std::optional<FontAtlas> RenderFreeType(void* face, int loadState, std::string_view fontName,
			uint32_t size, std::span<const GlyphRange> glyphRanges, int atlasWidth, int atlasHeight);
		
		int m_size;
		float m_lineHeight;
		float m_spaceAdvance;
		
		std::vector<Character> m_characters;
		std::vector<KerningPair> m_kerningPairs;
		
		struct AtlasData
		{
			int width = 0;
			int height = 0;
			uint8_t* data = nullptr;
			
			~AtlasData() noexcept
			{
				std::free(data);
			}
			
			AtlasData() = default;
			
			AtlasData(const AtlasData& other) noexcept;
			
			AtlasData(AtlasData&& other) noexcept
				: width(other.width), height(other.height), data(other.data)
			{
				other.data = nullptr;
			}
			
			AtlasData& operator=(AtlasData other) noexcept
			{
				width = other.width;
				height = other.height;
				std::swap(data, other.data);
				return *this;
			}
		};
		
		struct AtlasData m_atlasData;
	};
}
