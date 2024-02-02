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
} // namespace eg
