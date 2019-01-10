#pragma once


#include "API.hpp"

namespace eg
{
	EG_API bool ReadCompressedSection(std::istream& input, void* output, size_t outputSize);
	
	EG_API void WriteCompressedSection(std::ostream& output, const void* data, size_t dataSize);
	
	EG_API std::vector<char> ReadStreamContents(std::istream& stream);
	
	template <typename T, typename StreamTp, typename = std::enable_if_t<std::is_fundamental_v<T> || std::is_enum_v<T>>>
	inline void BinWrite(StreamTp& stream, T value)
	{
		stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}
	
	template <typename StreamTp>
	inline void BinWriteString(StreamTp& stream, std::string_view string)
	{
		BinWrite<uint16_t>(stream, string.size());
		stream.write(string.data(), string.size());
	}
	
	template <typename T, typename StreamTp, typename = std::enable_if_t<std::is_fundamental_v<T> || std::is_enum_v<T>>>
	inline T BinRead(StreamTp& stream)
	{
		T value;
		stream.read(reinterpret_cast<char*>(&value), sizeof(T));
		return value;
	}
	
	template <typename StreamTp>
	inline std::string BinReadString(StreamTp& stream)
	{
		uint16_t len = BinRead<uint16_t>(stream);
		std::string string(len, ' ');
		stream.read(string.data(), string.size());
		return string;
	}
}
