#include "String.hpp"

namespace eg
{
std::string_view TrimString(std::string_view input)
{
	if (input.empty())
		return input;

	size_t startWhitespace = 0;
	while (std::isspace(input[startWhitespace]))
	{
		startWhitespace++;
		if (startWhitespace >= input.size())
			return {};
	}

	size_t endWhitespace = input.size() - 1;
	while (std::isspace(input[endWhitespace]))
	{
		endWhitespace--;
	}

	return input.substr(startWhitespace, (endWhitespace + 1) - startWhitespace);
}

std::string Concat(std::initializer_list<std::string_view> list)
{
	size_t size = 0;
	for (std::string_view entry : list)
		size += entry.size();

	std::string result(size, ' ');

	char* output = result.data();
	for (std::string_view entry : list)
	{
		std::copy(entry.begin(), entry.end(), output);
		output += entry.size();
	}

	return result;
}

void SplitString(std::string_view string, char delimiter, std::vector<std::string_view>& partsOut)
{
	IterateStringParts(string, delimiter, [&](std::string_view part) { partsOut.push_back(part); });
}
} // namespace eg
