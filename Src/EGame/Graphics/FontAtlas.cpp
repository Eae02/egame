#include "FontAtlas.hpp"
#include "../IOUtils.hpp"
#include "../Log.hpp"
#include "../Platform/DynamicLibrary.hpp"
#include "../Platform/FileSystem.hpp"
#include "../String.hpp"
#include "../Utils.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <glm/glm.hpp>
#include <memory>
#include <stb_image.h>
#include <stb_rect_pack.h>
#include <utf8.h>

#ifndef EG_NO_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

namespace eg
{
struct KerningPairCompare
{
	bool operator()(const KerningPair& a, const KerningPair& b) const
	{
		if (a.first != b.first)
			return a.first < b.first;
		return a.second < b.second;
	}
};

const GlyphRange GlyphRange::ASCII{ 0x20, 0x7F };
const GlyphRange GlyphRange::LatinSupplement{ 0x80, 0xFF };
const GlyphRange GlyphRange::LatinExtended{ 0x100, 0x24F };

static const uint32_t DefaultChar = 0x25A1; // white square: □

#ifndef EG_NO_FREETYPE
static DynamicLibrary ftDynLibrary;

static FT_Library ftLibrary = nullptr;

namespace ft
{
#define DEF_FREETYPE_FUNC(name) decltype(&FT_##name) name;
DEF_FREETYPE_FUNC(Init_FreeType)
DEF_FREETYPE_FUNC(New_Memory_Face)
DEF_FREETYPE_FUNC(New_Face)
DEF_FREETYPE_FUNC(Set_Pixel_Sizes)
DEF_FREETYPE_FUNC(Load_Char)
DEF_FREETYPE_FUNC(Done_Face)
} // namespace ft

static inline bool MaybeInitFreeType()
{
	if (ftLibrary == nullptr)
	{
		std::string name = DynamicLibrary::PlatformFormat("freetype");
		if (!ftDynLibrary.Open(name.c_str()))
		{
			Log(LogLevel::Error, "fnt", "Error opening FreeType library.");
			return false;
		}

#define LOAD_FREETYPE_FUNC(name) ft::name = reinterpret_cast<decltype(ft::name)>(ftDynLibrary.GetSymbol("FT_" #name));
		LOAD_FREETYPE_FUNC(Init_FreeType)
		LOAD_FREETYPE_FUNC(New_Memory_Face)
		LOAD_FREETYPE_FUNC(New_Face)
		LOAD_FREETYPE_FUNC(Set_Pixel_Sizes)
		LOAD_FREETYPE_FUNC(Load_Char)
		LOAD_FREETYPE_FUNC(Done_Face)

		if (ft::Init_FreeType(&ftLibrary))
		{
			Log(LogLevel::Error, "fnt", "Error initializing FreeType.");
			return false;
		}
	}
	return true;
}
#endif

std::optional<FontAtlas> FontAtlas::Render(
	std::span<const char> data, uint32_t size, std::span<const GlyphRange> glyphRanges, int atlasWidth, int atlasHeight)
{
#ifdef EG_NO_FREETYPE
	return {};
#else
	if (!MaybeInitFreeType())
		return {};

	FT_Face face;
	FT_Error loadState =
		ft::New_Memory_Face(ftLibrary, reinterpret_cast<const FT_Byte*>(data.data()), data.size_bytes(), 0, &face);

	return RenderFreeType(face, loadState, "memory", size, glyphRanges, atlasWidth, atlasHeight);
#endif
}

std::optional<FontAtlas> FontAtlas::Render(
	const std::string& fontPath, uint32_t size, std::span<const GlyphRange> glyphRanges, int atlasWidth,
	int atlasHeight)
{
#ifdef EG_NO_FREETYPE
	return {};
#else
	if (!MaybeInitFreeType())
		return {};

	FT_Face face;
	FT_Error loadState = ft::New_Face(ftLibrary, fontPath.c_str(), 0, &face);

	return RenderFreeType(face, loadState, fontPath, size, glyphRanges, atlasWidth, atlasHeight);
#endif
}

#ifndef EG_NO_FREETYPE
std::optional<FontAtlas> FontAtlas::RenderFreeType(
	void* faceVP, int loadState, std::string_view fontName, uint32_t size, std::span<const GlyphRange> glyphRanges,
	int atlasWidth, int atlasHeight)
{
	if (loadState != 0)
	{
		if (loadState == FT_Err_Unknown_File_Format)
			Log(LogLevel::Error, "fnt", "Font '{0}' has an unknown file format.", fontName);
		else if (loadState == FT_Err_Cannot_Open_Stream)
			Log(LogLevel::Error, "fnt", "Cannot open font file: '{0}'.", fontName);
		else
			Log(LogLevel::Error, "fnt", "Unknown error reading font: '{0}'.", fontName);
		return {};
	}

	FT_Face face = reinterpret_cast<FT_Face>(faceVP);

	for (size_t i = 1; i < glyphRanges.size(); i++)
	{
		if (glyphRanges[i].start <= glyphRanges[i - 1].end)
		{
			Log(LogLevel::Error, "fnt", "Glyph ranges overlap or were not provided in ascending order.");
			return {};
		}
	}

	ft::Set_Pixel_Sizes(face, 0, size);

	FontAtlas atlas;
	atlas.m_size = ToInt(size);
	atlas.m_lineHeight = static_cast<float>(size);

	if (ft::Load_Char(face, ' ', FT_LOAD_DEFAULT) != 0)
	{
		Log(LogLevel::Error, "fnt", "'{0}' does not contain the space character.", fontName);
		ft::Done_Face(face);
		return {};
	}
	atlas.m_spaceAdvance = static_cast<float>(face->glyph->advance.x) / 64.0f;

	std::vector<stbrp_rect> rectangles;
	std::vector<std::unique_ptr<uint8_t[]>> bitmapCopies;

	int totalWidth = 0;
	int totalHeight = 0;
	const uint16_t PADDING = 2;

	auto RenderGlyph = [&](uint32_t c)
	{
		if (ft::Load_Char(face, c, FT_LOAD_RENDER))
		{
			char glyphUTF8[4];
			size_t glyphUTF8Len = utf8::unchecked::utf32to8(&c, &c + 1, glyphUTF8) - glyphUTF8;

			Log(LogLevel::Error, "fnt", "Failed to load glyph {0} ({1}) from '{2}'",
			    std::string_view(glyphUTF8, glyphUTF8Len), c, fontName);
			return;
		}

		stbrp_rect& rectangle = rectangles.emplace_back();
		rectangle.id = ToInt(atlas.m_characters.size());

		Character& character = atlas.m_characters.emplace_back();
		character.id = c;
		character.width = UnsignedNarrow<uint16_t>(face->glyph->bitmap.width);
		character.height = UnsignedNarrow<uint16_t>(face->glyph->bitmap.rows);
		character.xOffset = face->glyph->bitmap_left;
		character.yOffset = face->glyph->bitmap_top;
		character.xAdvance = static_cast<float>(face->glyph->advance.x) / 64.0f;

		rectangle.w = character.width + PADDING;
		rectangle.h = character.height + PADDING;

		const size_t bitmapSize = face->glyph->bitmap.width * face->glyph->bitmap.rows;
		std::unique_ptr<uint8_t[]> bitmapCopy = std::make_unique<uint8_t[]>(bitmapSize);
		std::memcpy(bitmapCopy.get(), face->glyph->bitmap.buffer, bitmapSize);
		bitmapCopies.push_back(std::move(bitmapCopy));

		totalWidth += rectangle.w;
		totalHeight += rectangle.h;
	};

	// Renders glyphs
	for (const GlyphRange& range : glyphRanges)
	{
		for (uint32_t c = range.start; c <= range.end; c++)
		{
			RenderGlyph(c);
		}
	}
	RenderGlyph(DefaultChar);

	ft::Done_Face(face);

	// Calculates the size of the atlas
	auto GetInitialSize = [](int res)
	{ return 1 << static_cast<int>(std::ceil(std::log2(std::sqrt(static_cast<double>(res))))); };
	if (atlasWidth == -1)
		atlasWidth = GetInitialSize(totalWidth);
	if (atlasHeight == -1)
		atlasHeight = GetInitialSize(totalHeight);

	// Packs glyph rectangles
	while (true)
	{
		std::unique_ptr<stbrp_node[]> nodes = std::make_unique<stbrp_node[]>(atlasWidth);

		stbrp_context stbrpCtx;
		stbrp_init_target(&stbrpCtx, atlasWidth, atlasHeight, nodes.get(), atlasWidth);

		if (stbrp_pack_rects(&stbrpCtx, rectangles.data(), ToInt(rectangles.size())))
			break;

		if (atlasWidth <= atlasHeight)
			atlasWidth *= 2;
		else
			atlasHeight *= 2;
	}

	atlas.m_atlasData.width = atlasWidth;
	atlas.m_atlasData.height = atlasHeight;
	atlas.m_atlasData.data = reinterpret_cast<uint8_t*>(std::calloc(1, atlasWidth * atlasHeight));

	// Copies data to the atlas
	for (const stbrp_rect& rectangle : rectangles)
	{
		Character& character = atlas.m_characters[rectangle.id];
		character.textureX = UnsignedNarrow<uint16_t>(ToUnsigned(rectangle.x)) + PADDING / 2;
		character.textureY = UnsignedNarrow<uint16_t>(ToUnsigned(rectangle.y)) + PADDING / 2;

		uint8_t* bitmapCopy = bitmapCopies[rectangle.id].get();
		for (uint32_t r = 0; r < character.height; r++)
		{
			std::memcpy(
				atlas.m_atlasData.data + (character.textureY + r) * atlasWidth + character.textureX,
				bitmapCopy + character.width * r, character.width);
		}
	}

	return atlas;
}
#endif

template <typename ReadLineCB, typename LoadImageCB>
std::optional<FontAtlas> FontAtlas::FromFNTInternal(ReadLineCB readLineCB, LoadImageCB loadImage, std::string_view name)
{
	FontAtlas atlas;

	std::vector<std::string_view> parts;

	std::string imageFileName;

	auto PrintMalformatted = [&] { Log(LogLevel::Error, "fnt", "Malformatted font file '{0}'", name); };

	bool hasDefaultChar = false;

	std::array<int, 4> padding = {};

	std::string line;
	while (readLineCB(line))
	{
		std::string_view lineView = TrimString(line);
		if (lineView.empty())
			continue;

		size_t firstSpace = lineView.find(' ');
		if (firstSpace == std::string_view::npos)
			continue;

		parts.clear();
		SplitString(lineView.substr(firstSpace + 1), ' ', parts);

		auto IsCommand = [&](const char* cmd) { return strncmp(lineView.data(), cmd, firstSpace) == 0; };

		auto GetPartValueI = [&](std::string_view partName)
		{
			for (const std::string_view part : parts)
			{
				if (part.size() >= partName.size() && part.compare(0, partName.size(), partName) == 0 &&
				    part[partName.size()] == '=')
				{
					std::string partS(&part[partName.size() + 1], part.data() + part.size());

					try
					{
						return std::stoi(partS);
					}
					catch (const std::exception& ex)
					{
						return INT_MIN;
					}
				}
			}
			return INT_MIN;
		};

		if (IsCommand("info"))
		{
			// for (const std::string_view part : parts)
			// {
			// 	auto [partName, partVal] = SplitStringOnce(part, '=');
			// 	if (partName == "padding")
			// 	{
			// 		std::vector<std::string_view> paddingParts;
			// 		SplitString(partVal, ',', paddingParts);
			// 		if (paddingParts.size() == 4)
			// 		{
			// 			for (size_t i = 0; i < 4; i++)
			// 			{
			// 				padding[i] = std::stoi(std::string(paddingParts[i]));
			// 			}
			// 		}
			// 	}
			// }
		}
		else if (IsCommand("common"))
		{
			atlas.m_lineHeight = static_cast<float>(GetPartValueI("lineHeight"));
			atlas.m_size = GetPartValueI("base");

			if (atlas.m_lineHeight == INT_MIN || atlas.m_size == INT_MIN)
			{
				PrintMalformatted();
				return {};
			}

			if (GetPartValueI("pages") > 1)
			{
				Log(LogLevel::Error, "fnt", "{0}: Multipage FNT is not supported.", name);
			}
		}
		else if (IsCommand("page"))
		{
			auto filePartIt = std::find_if(
				parts.begin(), parts.end(), [&](std::string_view part) { return part.starts_with("file="); });

			if (filePartIt == parts.end())
			{
				PrintMalformatted();
				return {};
			}

			std::string_view fileName = filePartIt->substr(5);
			if (!fileName.empty() && fileName[0] == '"' && fileName[fileName.size() - 1] == '"')
			{
				fileName = fileName.substr(1, fileName.size() - 2);
			}
			imageFileName = std::string(fileName);
		}
		else if (IsCommand("char"))
		{
			const int id = GetPartValueI("id");
			const int x = GetPartValueI("x");
			const int y = GetPartValueI("y");
			const int width = GetPartValueI("width");
			const int height = GetPartValueI("height");
			const int xOffset = GetPartValueI("xoffset");
			const int yOffset = GetPartValueI("yoffset");
			const int xAdvance = GetPartValueI("xadvance");
			if (id == INT_MIN || x == INT_MIN || y == INT_MIN || width == INT_MIN || height == INT_MIN ||
			    xOffset == INT_MIN || yOffset == INT_MIN || xAdvance == INT_MIN)
			{
				PrintMalformatted();
				return {};
			}

			float xAdvanceF = static_cast<float>(xAdvance - padding[0] - padding[2]);

			if (id == ' ')
			{
				atlas.m_spaceAdvance = xAdvanceF;
				continue;
			}

			if (id == DefaultChar)
				hasDefaultChar = true;

			Character& character = atlas.m_characters.emplace_back();
			character.id = ToUnsigned(id);
			character.textureX = UnsignedNarrow<uint16_t>(ToUnsigned(x));
			character.textureY = UnsignedNarrow<uint16_t>(ToUnsigned(y));
			character.width = UnsignedNarrow<uint16_t>(ToUnsigned(width));
			character.height = UnsignedNarrow<uint16_t>(ToUnsigned(height));
			character.xOffset = xOffset - padding[0];
			character.yOffset = static_cast<int>(std::round(atlas.m_lineHeight)) - (yOffset - padding[1]);
			character.xAdvance = xAdvanceF;
		}
		else if (IsCommand("kerning"))
		{
			const int first = GetPartValueI("first");
			const int second = GetPartValueI("second");
			if (first == INT_MIN || second == INT_MIN)
			{
				PrintMalformatted();
				return {};
			}

			atlas.m_kerningPairs.push_back({ ToUnsigned(first), ToUnsigned(second), GetPartValueI("amount") });
		}
	}

	if (!loadImage(imageFileName, atlas.m_atlasData))
		return {};

	atlas.m_lineHeight -= static_cast<float>(padding[1] + padding[3]);

	if (!hasDefaultChar)
	{
		eg::Log(eg::LogLevel::Warning, "fnt", "{0}: Default character (U+25A1) not included.", name);

		atlas.m_characters.push_back(Character{
			.id = DefaultChar,
			.width = 1,
			.height = 1,
			.xAdvance = atlas.m_spaceAdvance,
		});
	}

	std::sort(
		atlas.m_characters.begin(), atlas.m_characters.end(),
		[&](const Character& a, const Character& b) { return a.id < b.id; });

	std::sort(atlas.m_kerningPairs.begin(), atlas.m_kerningPairs.end(), KerningPairCompare());

	return atlas;
}

std::optional<FontAtlas> FontAtlas::FromFNTMemory(std::span<const char> fntData, std::span<const char> imgData)
{
	size_t curDataPos = 0;
	std::string_view dataStr(fntData.data(), fntData.size());

	return FromFNTInternal(
		[&](std::string& lineOut)
		{
			if (curDataPos == dataStr.size())
				return false;
			size_t endLn = dataStr.find('\n', curDataPos);
			if (endLn == std::string_view::npos)
			{
				lineOut = dataStr.substr(curDataPos);
				curDataPos = dataStr.size();
			}
			else
			{
				lineOut = dataStr.substr(curDataPos, endLn);
				curDataPos = endLn + 1;
			}
			return true;
		},
		[&](const std::string& imageName, struct AtlasData& data)
		{
			data.data = stbi_load_from_memory(
				reinterpret_cast<const stbi_uc*>(imgData.data()), ToInt(imgData.size()), &data.width, &data.height,
				nullptr, 1);

			if (data.data == nullptr)
			{
				Log(LogLevel::Error, "fnt", "Error loading image file from memory: {0}", stbi_failure_reason());
				return false;
			}

			return true;
		},
		"memory");
}

std::optional<FontAtlas> FontAtlas::FromFNT(const std::string& path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream.good())
	{
		Log(LogLevel::Error, "fnt", "Error opening font file '{0}'", path);
		return {};
	}

	return FromFNTInternal(
		[&](std::string& lineOut) { return !!std::getline(stream, lineOut); },
		[&](const std::string& imageName, struct AtlasData& data)
		{
			std::string fullImagePath = std::string(ParentPath(path)) + imageName;
			data.data = stbi_load(
				fullImagePath.c_str(), reinterpret_cast<int*>(&data.width), reinterpret_cast<int*>(&data.height),
				nullptr, 1);

			if (data.data == nullptr)
			{
				Log(LogLevel::Error, "fnt", "Error loading image file '{0}' referenced by '{1}': {2}", imageName, path,
			        stbi_failure_reason());
				return false;
			}

			return true;
		},
		path);
}

const Character& FontAtlas::GetCharacterOrDefault(uint32_t c) const
{
	if (const Character* ch = GetCharacter(c))
		return *ch;
	return *GetCharacter(DefaultChar);
}

const Character* FontAtlas::GetCharacter(uint32_t c) const
{
	auto it = std::lower_bound(
		m_characters.begin(), m_characters.end(), c, [&](const Character& a, uint32_t b) { return a.id < b; });

	if (it == m_characters.end() || it->id != c)
		return nullptr;
	return &*it;
}

int FontAtlas::GetKerning(uint32_t first, uint32_t second) const
{
	KerningPair dummyPair = { first, second, 0 };
	auto it = std::lower_bound(m_kerningPairs.begin(), m_kerningPairs.end(), dummyPair, KerningPairCompare());

	if (it == m_kerningPairs.end() || it->first != first || it->second != second)
		return 0;
	return it->amount;
}

glm::vec2 FontAtlas::GetTextExtents(std::string_view text) const
{
	uint16_t extentsY = 0;
	float x = 0;

	uint32_t prev = 0;
	for (auto it = text.begin(); it != text.end();)
	{
		const uint32_t c = utf8::unchecked::next(it);
		if (c == ' ')
		{
			x += m_spaceAdvance;
			continue;
		}

		const Character& fontChar = GetCharacterOrDefault(c);

		const int kerning = GetKerning(prev, c);

		x += fontChar.xAdvance + static_cast<float>(kerning);
		extentsY = std::max(extentsY, fontChar.height);
	}

	return glm::vec2(x, extentsY);
}

std::string FontAtlas::WordWrap(std::string_view text, float maxWidth) const
{
	std::string result;

	int64_t lineBegin = 0;
	int64_t lastBreak = -1;

	float x = 0;
	uint32_t prev = 0;
	for (auto it = text.begin(); it != text.end();)
	{
		const uint32_t c = utf8::unchecked::next(it);

		if (c == '\n')
		{
			result.append(text.begin() + lineBegin, it);
			lineBegin = it - text.begin();
			lastBreak = -1;
			x = 0;
			continue;
		}

		if (c == ' ')
		{
			lastBreak = it - text.begin();
		}

		const int kerning = GetKerning(prev, c);

		x += GetCharacterOrDefault(c).xAdvance + static_cast<float>(kerning);
		if (x > maxWidth && lastBreak != -1)
		{
			if (!result.empty())
				result.push_back('\n');
			result.append(text.begin() + lineBegin, text.begin() + lastBreak);
			lineBegin = lastBreak;
			lastBreak = -1;
			x = 0;
		}

		prev = c;
	}

	result.append(text.begin() + lineBegin, text.end());

	return result;
}

FontAtlas::AtlasData::AtlasData(const struct FontAtlas::AtlasData& other) noexcept
	: width(other.width), height(other.height)
{
	if (other.data != nullptr)
	{
		const size_t dataSize = width * height;
		data = static_cast<uint8_t*>(std::malloc(dataSize));
		std::memcpy(data, other.data, dataSize);
	}
}

struct AtlasHeader
{
	uint32_t size;
	float lineHeight;
	float spaceAdvance;
	uint32_t numCharacters;
	uint32_t numKerningPairs;
	uint32_t width;
	uint32_t height;
};

void FontAtlas::Serialize(MemoryWriter& writer) const
{
	writer.Write(AtlasHeader{
		.size = static_cast<uint32_t>(m_size),
		.lineHeight = m_lineHeight,
		.spaceAdvance = m_spaceAdvance,
		.numCharacters = UnsignedNarrow<uint32_t>(m_characters.size()),
		.numKerningPairs = UnsignedNarrow<uint32_t>(m_kerningPairs.size()),
		.width = ToUnsigned(m_atlasData.width),
		.height = ToUnsigned(m_atlasData.height),
	});

	writer.WriteMultiple<Character>(m_characters);
	writer.WriteMultiple<KerningPair>(m_kerningPairs);
	writer.WriteBytes({
		reinterpret_cast<const char*>(m_atlasData.data),
		static_cast<size_t>(m_atlasData.width * m_atlasData.height),
	});
}

FontAtlas FontAtlas::Deserialize(MemoryReader& reader)
{
	FontAtlas atlas;

	AtlasHeader header = reader.Read<AtlasHeader>();
	atlas.m_size = header.size;
	atlas.m_lineHeight = header.lineHeight;
	atlas.m_spaceAdvance = header.spaceAdvance;
	atlas.m_atlasData.width = header.width;
	atlas.m_atlasData.height = header.height;
	size_t dataBytes = static_cast<size_t>(atlas.m_atlasData.width) * static_cast<size_t>(atlas.m_atlasData.height);

	atlas.m_characters.resize(header.numCharacters);
	atlas.m_kerningPairs.resize(header.numKerningPairs);
	atlas.m_atlasData.data = static_cast<uint8_t*>(std::malloc(dataBytes));

	reader.ReadToSpan<Character>(atlas.m_characters);
	reader.ReadToSpan<KerningPair>(atlas.m_kerningPairs);

	std::copy_n(reader.ReadBytes(dataBytes).data(), dataBytes, reinterpret_cast<char*>(atlas.m_atlasData.data));

	return atlas;
}
} // namespace eg
