#include "Utils.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <SDL2/SDL.h>

namespace eg
{
	bool detail::devMode = false;
	
	std::string ReadableSize(uint64_t size)
	{
		std::ostringstream stream;
		stream << std::setprecision(3);
		if (size < 1024)
			stream << size << "B";
		else if (size < 1024 * 1024)
			stream << (size / (double)1024) << "KiB";
		else if (size < 1024 * 1024 * 1024)
			stream << (size / (double)(1024 * 1024)) << "MiB";
		else
			stream << (size / (double)(1024 * 1024 * 1024)) << "GiB";
		return stream.str();
	}
	
	std::string_view TrimString(std::string_view input)
	{
		if (input.empty())
			return input;
		
		size_t startWhitespace = 0;
		while (std::isspace(input[startWhitespace]))
		{
			startWhitespace++;
			if (startWhitespace >= input.size())
				return { };
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
		IterateStringParts(string, delimiter, [&] (std::string_view part)
		{
			partsOut.push_back(part);
		});
	}
	
	void ReleasePanic(const std::string& message)
	{
		std::cerr << message << std::endl;
		
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Runtime Error", message.c_str(), nullptr);
		
		std::abort();
	}
	
	uint64_t HashFNV1a64(std::string_view s)
	{
		constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
		constexpr uint64_t FNV_PRIME = 1099511628211ull;
		
		uint64_t h = FNV_OFFSET_BASIS;
		for (char c : s)
		{
			h ^= static_cast<uint8_t>(c);
			h *= FNV_PRIME;
		}
		return h;
	}
	
	uint32_t HashFNV1a32(std::string_view s)
	{
		constexpr uint32_t FNV_OFFSET_BASIS = 2166136261;
		constexpr uint32_t FNV_PRIME = 16777619;
		
		uint32_t h = FNV_OFFSET_BASIS;
		for (char c : s)
		{
			h ^= static_cast<uint8_t>(c);
			h *= FNV_PRIME;
		}
		return h;
	}
}
