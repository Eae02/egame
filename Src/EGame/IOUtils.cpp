#include "IOUtils.hpp"
#include "Utils.hpp"

#include <fstream>
#include <zlib.h>
#include <cassert>
#include <list>

namespace eg
{
	std::vector<char> ReadStreamContents(std::istream& stream)
	{
		std::vector<char> result;
		
		char data[4096];
		while (!stream.eof())
		{
			stream.read(data, sizeof(data));
			result.insert(result.end(), data, data + stream.gcount());
		}
		
		return result;
	}
	
	MemoryStreambuf::pos_type MemoryStreambuf::seekoff(MemoryStreambuf::off_type off,
		std::ios_base::seekdir dir, std::ios_base::openmode which)
	{
		if(dir == std::ios_base::cur)
			gbump(off);
		else if(dir == std::ios_base::end)
			setg(m_begin, m_end + off, m_end);
		else if(dir == std::ios_base::beg)
			setg(m_begin, m_begin + off, m_end);
		
		return gptr() - eback();
	}
	
	MemoryStreambuf::pos_type MemoryStreambuf::seekpos(std::streampos pos, std::ios_base::openmode mode)
	{
		return seekoff(pos - static_cast<pos_type>(static_cast<off_type>(0)), std::ios_base::beg, mode);
	}
	
	bool ReadCompressedSection(std::istream& input, void* output, size_t outputSize)
	{
		const uint64_t compressedSize = BinRead<uint64_t>(input);
		
		z_stream inflateStream = { };
		if (inflateInit(&inflateStream) != Z_OK)
		{
			EG_PANIC("Error initializing ZLIB");
		}
		
		int status;
		std::array<char, 256> outBuffer;
		std::array<char, 256> inBuffer;
		
		long charsLeft = compressedSize;
		long outputcharsLeft = outputSize;
		
		char* outputPtr = reinterpret_cast<char*>(output);
		
		//Inflates the data 256 chars at a time
		do
		{
			long charsToRead = std::min<long>(inBuffer.size(), charsLeft);
			input.read(inBuffer.data(), charsToRead);
			
			assert(input.gcount() == charsToRead);
			
			charsLeft -= charsToRead;
			
			inflateStream.avail_in = static_cast<uInt>(charsToRead);
			inflateStream.next_in = reinterpret_cast<Bytef*>(inBuffer.data());
			
			if (inflateStream.avail_in == 0)
				break;
			
			do
			{
				inflateStream.avail_out = outBuffer.size();
				inflateStream.next_out = reinterpret_cast<Bytef*>(outBuffer.data());
				
				status = inflate(&inflateStream, Z_NO_FLUSH);
				EG_ASSERT(status != Z_STREAM_ERROR);
				
				if (status == Z_MEM_ERROR)
					std::abort();
				if (status == Z_DATA_ERROR || status == Z_NEED_DICT)
					return false;
				
				long charsDecompressed = static_cast<long>(outBuffer.size()) - static_cast<long>(inflateStream.avail_out);
				if (outputcharsLeft < charsDecompressed)
					return false;
				
				std::copy_n(outBuffer.begin(), charsDecompressed, outputPtr);
				
				outputPtr += charsDecompressed;
				outputcharsLeft -= charsDecompressed;
			}
			while (inflateStream.avail_out == 0);
		}
		while (status != Z_STREAM_END && charsLeft > 0);
		
		inflateEnd(&inflateStream);
		return true;
	}
	
	void WriteCompressedSection(std::ostream& output, const void* data, size_t dataSize)
	{
		z_stream deflateStream = { };
		
		deflateStream.avail_in = static_cast<uInt>(dataSize);
		deflateStream.next_in = reinterpret_cast<const Bytef*>(data);
		
		if (deflateInit(&deflateStream, Z_DEFAULT_COMPRESSION) != Z_OK)
		{
			EG_PANIC("Error initializing ZLIB");
		}
		
		std::list<std::array<char, 256>> compressedData;
		uInt lastPageUnusedchars;
		
		//Deflates and writes the data 256 chars at a time
		while (true)
		{
			auto& outBuffer = compressedData.emplace_back();
			
			deflateStream.avail_out = outBuffer.size();
			deflateStream.next_out = reinterpret_cast<Bytef*>(outBuffer.data());
			
			int status = deflate(&deflateStream, Z_FINISH);
			EG_ASSERT(status != Z_STREAM_ERROR);
			
			if (deflateStream.avail_out != 0)
			{
				lastPageUnusedchars = deflateStream.avail_out;
				break;
			}
		}
		
		deflateEnd(&deflateStream);
		
		BinWrite<uint64_t>(output, compressedData.size() * 256 - lastPageUnusedchars);
		std::for_each(compressedData.begin(), std::prev(compressedData.end()), [&] (const std::array<char, 256>& page)
		{
			output.write(page.data(), page.size());
		});
		output.write(compressedData.back().data(), 256 - lastPageUnusedchars);
	}
	
	std::vector<char> Compress(const void* data, size_t dataSize)
	{
		z_stream deflateStream = { };
		
		deflateStream.avail_in = static_cast<uInt>(dataSize);
		deflateStream.next_in = reinterpret_cast<const Bytef*>(data);
		
		if (deflateInit(&deflateStream, Z_BEST_COMPRESSION) != Z_OK)
		{
			EG_PANIC("Error initializing ZLIB");
		}
		
		std::vector<char> output;
		char outBuffer[256];
		
		//Deflates and writes the data 256 chars at a time
		while (true)
		{
			deflateStream.avail_out = sizeof(outBuffer);
			deflateStream.next_out = reinterpret_cast<Bytef*>(outBuffer);
			
			int status = deflate(&deflateStream, Z_FINISH);
			assert(status != Z_STREAM_ERROR);
			
			output.insert(output.end(), outBuffer, outBuffer + (sizeof(outBuffer) - deflateStream.avail_out));
			
			if (deflateStream.avail_out != 0)
				break;
		}
		
		deflateEnd(&deflateStream);
		
		return output;
	}
	
	bool Decompress(const void* input, size_t inputSize, void* output, size_t outputSize)
	{
		z_stream inflateStream = { };
		if (inflateInit(&inflateStream) != Z_OK)
		{
			EG_PANIC("Error initializing ZLIB");
		}
		
		inflateStream.avail_in = (uInt)inputSize;
		inflateStream.next_in = reinterpret_cast<const Bytef*>(input);
		inflateStream.avail_out = (uInt)outputSize;
		inflateStream.next_out = reinterpret_cast<Bytef*>(output);
		
		int status = inflate(&inflateStream, Z_NO_FLUSH);
		
		inflateEnd(&inflateStream);
		
		if (status == Z_MEM_ERROR || status == Z_STREAM_ERROR)
			std::abort();
		return status == Z_STREAM_END;
	}
	
	static const char* Base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	
	std::vector<char> Base64Decode(std::string_view in)
	{
		std::vector<char> out;
		
		std::vector<int> translate(256, -1);
		for (int i = 0; i < 64; i++)
		{
			translate[Base64Chars[i]] = i;
		}
		
		int val = 0;
		int valb = -8;
		for (char c : in)
		{
			if (translate[c] == -1)
				break;
			val = (val << 6) + translate[c];
			valb += 6;
			if (valb >= 0)
			{
				out.push_back(char((val >> valb) & 0xFF));
				valb -= 8;
			}
		}
		return out;
	}
}
