#pragma once

#include "API.hpp"
#include "Assert.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace eg
{
	EG_API std::vector<char> ReadStreamContents(std::istream& stream);
	
	class EG_API MemoryStreambuf : public std::streambuf
	{
	public:
		MemoryStreambuf(const char* begin, const char* end)
			: m_begin(const_cast<char*>(begin)), m_end(const_cast<char*>(end))
		{
			setg(m_begin, m_begin, m_end);
		}
		
		inline explicit MemoryStreambuf(std::span<const char> data)
			: MemoryStreambuf(data.data(), data.data() + data.size()) { }
		
		virtual pos_type seekoff(off_type off, std::ios_base::seekdir dir,
			std::ios_base::openmode which = std::ios_base::in) override;
		
		virtual pos_type seekpos(std::streampos pos, std::ios_base::openmode mode) override;
		
	private:
		char* m_begin;
		char* m_end;
	};
	
	template <typename T, typename StreamTp, typename = std::enable_if_t<std::is_fundamental_v<T> || std::is_enum_v<T>>>
	inline void BinWrite(StreamTp& stream, T value)
	{
		stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}
	
	template <typename StreamTp>
	inline void BinWriteString(StreamTp& stream, std::string_view string)
	{
		if (string.size() > UINT16_MAX)
			EG_PANIC("String passed to BinWriteString was too long");
		BinWrite(stream, static_cast<uint16_t>(string.size()));
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
