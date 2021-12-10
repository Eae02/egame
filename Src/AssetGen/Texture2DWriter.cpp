#include "Texture2DWriter.hpp"
#include "../EGame/Utils.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Graphics/ImageLoader.hpp"
#include "../EGame/Graphics/AbstractionHL.hpp"

#include <stb_image_resize.h>
#include <stb_dxt.h>

namespace eg::asset_gen
{
	static const std::pair<std::string_view, Format> formatNames[] = 
	{
		{ "r8", Format::R8_UNorm },
		{ "rgba8", Format::R8G8B8A8_UNorm },
		{ "bc1", Format::BC1_RGBA_UNorm },
		{ "bc3", Format::BC3_UNorm },
		{ "bc4", Format::BC4_UNorm },
		{ "bc5", Format::BC5_UNorm }
	};
	
	void Texture2DWriter::ParseYAMLSettings(const YAML::Node& node)
	{
		//Reads the image format
		if (const YAML::Node& formatNode = node["format"])
		{
			std::string formatName = formatNode.as<std::string>();
			
			bool found = false;
			for (const std::pair<std::string_view, Format>& formatCandidate : formatNames)
			{
				if (StringEqualCaseInsensitive(formatName, formatCandidate.first))
				{
					found = true;
					m_format = formatCandidate.second;
					break;
				}
			}
			
			if (!found)
			{
				Log(LogLevel::Error, "as", "Unknown texture format: {0}.", formatName);
			}
		}
		
		if (const YAML::Node& mipShiftNode = node["mipShift"])
		{
			m_mipShiftLow = mipShiftNode["low"].as<int>(0);
			m_mipShiftMedium = mipShiftNode["medium"].as<int>(0);
			m_mipShiftHigh = mipShiftNode["high"].as<int>(0);
		}
		
		m_width = node["width"].as<int>(-1);
		m_height = node["height"].as<int>(-1);
		
		m_isSRGB = node["srgb"].as<bool>(false);
		
		m_numMipLevels = node["mipLevels"].as<int>(0);
		
		m_linearFiltering = node["filtering"].as<std::string>("linear") == "linear";
		m_dxtDither = node["dither"].as<bool>(false);
		m_anisotropicFiltering = node["enableAnistropy"].as<bool>(true);
		m_useGlobalDownscale = node["useGlobalDownscale"].as<bool>(false);
	}
	
	void Texture2DWriter::ProcessMipLevel(const uint8_t* imageData, int width, int height, int mode)
	{
		const int alpha = m_format == eg::Format::BC3_UNorm;
		
		int bytesPerBlock;
		switch (m_format)
		{
		case eg::Format::BC1_RGBA_UNorm:
		case eg::Format::BC4_UNorm:
			bytesPerBlock = 8;
			break;
		case eg::Format::BC3_UNorm:
		case eg::Format::BC5_UNorm:
			bytesPerBlock = 16;
			break;
		case eg::Format::R8G8B8A8_UNorm:
		{
			m_data.emplace_back(imageData, width * height * 4);
			return;
		}
		case eg::Format::R8_UNorm:
		{
			m_data.emplace_back(imageData, width * height);
			return;
		}
		default: EG_PANIC("Unexpected format")
		}
		
		int numBlocks = ((width + 3) / 4) * ((height + 3) / 4);
		
		const size_t outputBytes = numBlocks * bytesPerBlock;
		std::unique_ptr<uint8_t, FreeDel> outputDataUP((uint8_t*)std::malloc(outputBytes));
		uint8_t* outputBuffer = outputDataUP.get();
		
		switch (m_format)
		{
		case eg::Format::BC1_RGBA_UNorm:
		case eg::Format::BC3_UNorm:
		{
			uint8_t inputBuffer[4][4][4];
			for (int y = 0 ; y < height; y += 4)
			{
				for (int x = 0; x < width; x += 4)
				{
					//Copies block pixels to the input buffer
					for (int by = 0; by < 4; by++)
					{
						int ty = y + by;
						for (int bx = 0; bx < 4; bx++)
						{
							int tx = x + bx;
							if (tx >= width || ty >= height)
							{
								std::memset(inputBuffer[by][bx], 0, 4);
							}
							else
							{
								std::copy_n(imageData + (ty * width + tx) * 4, 4, inputBuffer[by][bx]);
							}
						}
					}
					
					stb_compress_dxt_block(outputBuffer, inputBuffer[0][0], alpha, mode);
					outputBuffer += bytesPerBlock;
				}
			}
			
			break;
		}
		case eg::Format::BC4_UNorm:
		{
			uint8_t inputBuffer[4][4];
			
			for (int y = 0 ; y < height; y += 4)
			{
				for (int x = 0; x < width; x += 4)
				{
					//Copies block pixels to the input buffer
					for (int by = 0; by < 4; by++)
					{
						int ty = y + by;
						for (int bx = 0; bx < 4; bx++)
						{
							int tx = x + bx;
							if (tx >= width || ty >= height)
								inputBuffer[by][bx] = 0;
							else
								inputBuffer[by][bx] = imageData[ty * width + tx];
						}
					}
					
					stb_compress_bc4_block(outputBuffer, inputBuffer[0]);
					outputBuffer += bytesPerBlock;
				}
			}
			
			break;
		}
		case eg::Format::BC5_UNorm:
		{
			uint8_t inputBuffer[4][4][2];
			
			for (int y = 0 ; y < height; y += 4)
			{
				for (int x = 0; x < width; x += 4)
				{
					//Copies block pixels to the input buffer
					for (int by = 0; by < 4; by++)
					{
						int ty = y + by;
						for (int bx = 0; bx < 4; bx++)
						{
							int tx = x + bx;
							if (tx >= width || ty >= height)
							{
								std::memset(inputBuffer[by][bx], 0, 2);
							}
							else
							{
								std::copy_n(imageData + (ty * width + tx) * 4, 2, inputBuffer[by][bx]);
							}
						}
					}
					
					stb_compress_bc5_block(outputBuffer, inputBuffer[0][0]);
					outputBuffer += bytesPerBlock;
				}
			}
			
			break;
		}
		default: EG_PANIC("Unexpected format")
		}
		
		m_data.emplace_back(outputDataUP.get(), outputBytes);
		m_freeDelUP.push_back(std::move(outputDataUP));
	}
	
	int NextMipSize(int prevSize)
	{
		return std::max(prevSize / 2, 1);
	}
	
	template <bool Linear>
	void GenerateNextMip(const uint8_t* src, uint8_t* dest, int width, int height, int numChannels)
	{
		int nextWidth = NextMipSize(width);
		int nextHeight = NextMipSize(height);
		
		auto GetSrcPixel = [&] (int x, int y, int c)
		{
			uint32_t v = src[((y * width) + x) * numChannels + c];
			if (!Linear && c < 3)
				v = eg::SRGBToLinear(v / 255.0f) * 255.0f;
			return v;
		};
		
		//For each pixel in the next image, averages the pixels from the previous 2x2 block
		for (int y = 0; y < nextHeight; y++)
		{
			for (int x = 0; x < nextWidth; x++)
			{
				for (int c = 0; c < numChannels; c++)
				{
					uint32_t sumIntensity = 0;
					for (int oy = 0; oy < 2; oy++)
					{
						for (int ox = 0; ox < 2; ox++)
						{
							sumIntensity += GetSrcPixel(x * 2 + ox, y * 2 + oy, c);
						}
					}
					
					*dest = static_cast<uint8_t>(sumIntensity / 4);
					if (!Linear && c < 3)
						*dest = eg::LinearToSRGB(*dest / 255.0f) * 255.0f;
					dest++;
				}
			}
		}
	}
	
	bool Texture2DWriter::AddLayer(std::istream& imageStream, std::string_view fileName)
	{
		std::string messagePrefix;
		if (!fileName.empty())
			messagePrefix = Concat({"Texture '", fileName, "': "});
		
		ImageLoader loader(imageStream);
		
		if (m_width == -1)
			m_width = loader.Width();
		if (m_height == -1)
			m_height = loader.Height();
		
		if (IsCompressedFormat(m_format) && (m_width % 4 != 0 || m_height % 4 != 0))
		{
			Log(LogLevel::Warning, "as", "{0}Compressed textures must have a size divisible by 4 (got {1}x{2}).",
				messagePrefix, m_width, m_height);
			return false;
		}
		
		if (m_numMipLevels == 0)
		{
			m_numMipLevels = Texture::MaxMipLevels((uint32_t)std::max(m_width, m_height));
			
			//Removes the lowest 2 mip levels in the case of compressed textures, since these will be smaller than 4x4
			if (IsCompressedFormat(m_format))
				m_numMipLevels = std::max(m_numMipLevels - 2, 1);
		}
		
		int loadChannels = 4;
		if (m_format == Format::BC4_UNorm || m_format == Format::R8_UNorm)
			loadChannels = 1;
		
		//Loads the image
		std::unique_ptr<uint8_t, FreeDel> firstLayerDataUP = loader.Load(loadChannels);
		if (!firstLayerDataUP)
			return false;
		uint8_t* firstLayerData = firstLayerDataUP.get();
		
		bool isCompressed = m_format != Format::R8_UNorm && m_format != Format::R8G8B8A8_UNorm;
		if (!isCompressed)
		{
			m_freeDelUP.push_back(std::move(firstLayerDataUP));
		}
		
		//Resizes the image if the size doesn't match
		if (m_width != loader.Width() || m_height != loader.Height())
		{
			Log(LogLevel::Warning, "as", "{0}Inconsistent texture array resolution, layer '{1}' will be resized to {2}x{3}.",
				messagePrefix, m_numLayers, m_width, m_height);
			
			uint8_t* newData = reinterpret_cast<uint8_t*>(std::malloc(m_width * m_height * loadChannels));
			m_freeDelUP.emplace_back(newData);
			
			if (m_isSRGB)
			{
				stbir_resize_uint8_srgb(firstLayerData, loader.Width(), loader.Height(), 0, newData, m_width, m_height,
				                        0, loadChannels, 3, loadChannels == 1 ? STBIR_ALPHA_CHANNEL_NONE : 0);
			}
			else
			{
				stbir_resize_uint8(firstLayerData, loader.Width(), loader.Height(), 0, newData, m_width, m_height, 0,
				                   loadChannels);
			}
			
			firstLayerData = newData;
		}
		
		//Allocates memory for generation of mipmaps
		std::unique_ptr<uint8_t, FreeDel> mipDataUP;
		uint8_t* nextMipData = nullptr;
		if (m_numMipLevels > 1)
		{
			size_t firstMipBytes = (m_width * m_height * loadChannels);
			mipDataUP.reset((uint8_t*)std::malloc(firstMipBytes * 2));
			nextMipData = mipDataUP.get();
			if (!isCompressed)
			{
				m_freeDelUP.push_back(std::move(mipDataUP));
			}
		}
		
		int dxtMode = m_dxtHighQuality ? STB_DXT_HIGHQUAL : STB_DXT_NORMAL;
		if (m_dxtDither)
			dxtMode |= STB_DXT_DITHER;
		
		ProcessMipLevel(firstLayerData, m_width, m_height, dxtMode);
		
		int mipWidth = m_width;
		int mipHeight = m_height;
		uint8_t* srcData = firstLayerData;
		for (int i = 1; i < m_numMipLevels; i++)
		{
			//Generates the next mip level
			if (m_isSRGB)
				GenerateNextMip<false>(srcData, nextMipData, mipWidth, mipHeight, loadChannels);
			else
				GenerateNextMip<true>(srcData, nextMipData, mipWidth, mipHeight, loadChannels);
			
			mipWidth = NextMipSize(mipWidth);
			mipHeight = NextMipSize(mipHeight);
			
			ProcessMipLevel(nextMipData, mipWidth, mipHeight, dxtMode);
			
			srcData = nextMipData;
			nextMipData += mipWidth * mipHeight * loadChannels;
		}
		
		m_numLayers++;
		
		return true;
	}
	
	bool Texture2DWriter::Write(std::ostream& stream) const
	{
		Format realFormat = m_format;
		if (m_isSRGB)
		{
			switch (m_format)
			{
			case Format::R8G8B8A8_UNorm:
				realFormat = Format::R8G8B8A8_sRGB;
				break;
			case Format::BC1_RGBA_UNorm:
				realFormat = Format::BC1_RGBA_sRGB;
				break;
			case Format::BC3_UNorm:
				realFormat = Format::BC3_sRGB;
				break;
			default:
				Log(LogLevel::Error, "as", "sRGB is not supported for the selected format.");
				return false;
			}
		}
		
		if (m_isCubeMap && (m_numLayers % 6) != 0)
		{
			Log(LogLevel::Error, "as", "Cube map textures must have a layer count that is a multiple of 6.");
			return false;
		}
		
		if (m_isCubeMap && m_width != m_height)
		{
			Log(LogLevel::Error, "as", "Cube map textures must have width = height.");
			return false;
		}
		
		BinWrite(stream, (uint32_t)m_numLayers);
		BinWrite(stream, (uint32_t)realFormat);
		
		uint32_t flags = (uint32_t)m_linearFiltering |
			(uint32_t)m_anisotropicFiltering << 1U |
			(uint32_t)m_useGlobalDownscale << 2U |
			(uint32_t)m_isArrayTexture << 3U |
			(uint32_t)m_isCubeMap << 4U | 
			(uint32_t)m_is3D << 5U;
		BinWrite(stream, (uint32_t)flags);
		
		BinWrite(stream, (uint32_t)m_mipShiftLow);
		BinWrite(stream, (uint32_t)m_mipShiftMedium);
		BinWrite(stream, (uint32_t)m_mipShiftHigh);
		
		BinWrite(stream, (uint32_t)m_numMipLevels);
		BinWrite(stream, (uint32_t)m_width);
		BinWrite(stream, (uint32_t)m_height);
		
		for (std::span<const uint8_t> data : m_data)
		{
			stream.write(reinterpret_cast<const char*>(data.data()), data.size());
		}
		
		return true;
	}
}
