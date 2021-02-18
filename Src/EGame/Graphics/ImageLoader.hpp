#pragma once

#include "../API.hpp"
#include "../Utils.hpp"

#include <istream>
#include <memory>
#include <cstdint>

namespace eg
{
	/**
	 * Loads PNG/JPG/TGA/BMP/GIF images from a stream.
	 */
	class EG_API ImageLoader
	{
	public:
		explicit ImageLoader(std::istream& stream);
		
		/**
		 * Loads the image data.
		 * @param numChannels The desired number of channels in the output.
		 * @return The loaded image data, or null if an error occurred.
		 */
		std::unique_ptr<uint8_t, FreeDel> Load(int numChannels);
		
		int Width() const
		{
			return m_width;
		}
		
		int Height() const
		{
			return m_height;
		}
		
	private:
		std::istream* m_stream;
		std::streampos m_startPos;
		int m_width;
		int m_height;
	};
}
