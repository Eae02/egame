#include "ImageLoader.hpp"
#include "../Log.hpp"

#include <stb_image.h>

namespace eg
{
	static int STBIRead(void* user, char* data, int size)
	{
		auto* stream = reinterpret_cast<std::istream*>(user);
		stream->read(data, size);
		return static_cast<int>(stream->gcount());
	}
	
	static void STBISkip(void* user, int n)
	{
		reinterpret_cast<std::istream*>(user)->seekg(n, std::ios::cur);
	}
	
	static int STBIEof(void* user)
	{
		return reinterpret_cast<std::istream*>(user)->eof();
	}
	
	static const stbi_io_callbacks ioCallbacks = { STBIRead, STBISkip, STBIEof };
	
	ImageLoader::ImageLoader(std::istream& stream)
		: m_stream(&stream)
	{
		m_startPos = stream.tellg();
		stbi_info_from_callbacks(&ioCallbacks, &stream, &m_width, &m_height, nullptr);
	}
	
	std::unique_ptr<uint8_t, FreeDel> ImageLoader::Load(int numChannels)
	{
		m_stream->seekg(m_startPos);
		
		int w, h;
		uint8_t* data = stbi_load_from_callbacks(&ioCallbacks, &m_stream, &w, &h, nullptr, numChannels);
		if (data == nullptr)
		{
			Log(LogLevel::Error, "img", "{0}", stbi_failure_reason());
		}
		
		return std::unique_ptr<uint8_t, FreeDel>(data);
	}
}
