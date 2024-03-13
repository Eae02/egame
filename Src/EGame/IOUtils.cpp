#include "IOUtils.hpp"
#include "Utils.hpp"

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

MemoryStreambuf::pos_type MemoryStreambuf::seekoff(
	MemoryStreambuf::off_type off, std::ios_base::seekdir dir, std::ios_base::openmode which)
{
	if (dir == std::ios_base::cur)
		gbump(ToInt(off));
	else if (dir == std::ios_base::end)
		setg(m_begin, m_end + off, m_end);
	else if (dir == std::ios_base::beg)
		setg(m_begin, m_begin + off, m_end);

	return gptr() - eback();
}

MemoryStreambuf::pos_type MemoryStreambuf::seekpos(std::streampos pos, std::ios_base::openmode mode)
{
	return seekoff(pos - static_cast<pos_type>(static_cast<off_type>(0)), std::ios_base::beg, mode);
}

void MemoryWriter::WriteBytes(std::span<const char> data)
{
	size_t dataOffset = 0;

	while (dataOffset < data.size())
	{
		const size_t bytesToWrite = std::min(data.size() - dataOffset, BYTES_PER_BLOCK - m_lastBlockLength);
		std::memcpy(m_blocks.back().data() + m_lastBlockLength, data.data() + dataOffset, bytesToWrite);
		m_lastBlockLength += bytesToWrite;
		dataOffset += bytesToWrite;

		if (m_lastBlockLength == BYTES_PER_BLOCK)
		{
			m_blocks.emplace_back();
			m_lastBlockLength = 0;
		}
	}

	m_length += data.size();
}

void MemoryWriter::CopyToStream(std::ostream& stream) const
{
	for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it)
	{
		const bool isLast = std::next(it) == m_blocks.end();
		stream.write(it->data(), isLast ? m_lastBlockLength : BYTES_PER_BLOCK);
	}
}

std::vector<char> MemoryWriter::ToVector() const
{
	std::vector<char> v;
	v.reserve(m_length);
	for (auto it = m_blocks.begin(); it != m_blocks.end(); ++it)
	{
		const bool isLast = std::next(it) == m_blocks.end();
		const size_t blockLen = isLast ? m_lastBlockLength : BYTES_PER_BLOCK;
		v.insert(v.end(), it->begin(), it->begin() + blockLen);
	}
	return v;
}
} // namespace eg
