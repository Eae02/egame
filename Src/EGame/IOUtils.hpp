#pragma once

#include "API.hpp"
#include "Assert.hpp"

#include <cstddef>
#include <cstdint>
#include <list>
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
		: MemoryStreambuf(data.data(), data.data() + data.size())
	{
	}

	virtual pos_type seekoff(
		off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which = std::ios_base::in) override;

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

class MemoryReader
{
public:
	explicit MemoryReader(std::span<const char> _data) : data(_data) {}

	template <typename T, bool SkipCheckTrivial = false>
	T Read()
	{
		static_assert(std::is_trivial_v<T> || SkipCheckTrivial);
		EG_ASSERT(dataOffset + sizeof(T) <= data.size());
		T value;
		std::memcpy(&value, data.data() + dataOffset, sizeof(T));
		dataOffset += sizeof(T);
		return value;
	}

	template <typename T, bool SkipCheckTrivial = false>
	void ReadToSpan(std::span<T> values)
	{
		static_assert(std::is_trivial_v<T> || SkipCheckTrivial);
		EG_ASSERT(dataOffset + values.size_bytes() <= data.size());
		T value;
		std::memcpy(values.data(), data.data() + dataOffset, values.size_bytes());
		dataOffset += values.size_bytes();
	}

	std::string_view ReadString()
	{
		uint16_t len = Read<uint16_t>();
		EG_ASSERT(dataOffset + len <= data.size());
		std::string_view s(data.data() + dataOffset, len);
		dataOffset += len;
		return s;
	}

	std::span<const char> ReadBytes(size_t n)
	{
		auto result = data.subspan(dataOffset, n);
		dataOffset += n;
		return result;
	}

	size_t dataOffset = 0;
	std::span<const char> data;
};

class MemoryWriter
{
public:
	MemoryWriter() : m_blocks(1) {}

	template <typename T, bool SkipCheckTrivial = false>
	void Write(T value)
	{
		static_assert(std::is_trivial_v<T> || SkipCheckTrivial);
		WriteBytes(std::span<const char>(reinterpret_cast<const char*>(&value), sizeof(T)));
	}

	template <typename T, bool SkipCheckTrivial = false>
	void WriteMultiple(std::span<const T> value)
	{
		static_assert(std::is_trivial_v<T> || SkipCheckTrivial);
		WriteBytes(std::span<const char>(reinterpret_cast<const char*>(value.data()), value.size_bytes()));
	}

	void WriteString(std::string_view string)
	{
		if (string.size() > UINT16_MAX)
			EG_PANIC("String passed to WriteString was too long");
		Write(static_cast<uint16_t>(string.size()));
		WriteBytes(string);
	}

	void WriteBytes(std::span<const char> data);

	void CopyToStream(std::ostream& stream) const;

	std::vector<char> ToVector() const;

private:
	static constexpr size_t BYTES_PER_BLOCK = 16 * 1024;

	std::list<std::array<char, BYTES_PER_BLOCK>> m_blocks;

	size_t m_lastBlockLength = 0;
	size_t m_length = 0;
};
} // namespace eg
