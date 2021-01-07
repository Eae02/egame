#include "Utils.hpp"
#include "Core.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <stack>

#ifndef __EMSCRIPTEN__
#include <SDL.h>
#endif

namespace eg
{
	bool detail::devMode = false;
	bool detail::blockGraphicsAPIDestroy = false;
	
	void ParseCommandLineArgs(RunConfig& runConfig, int argc, char** argv)
	{
		if (argc == 2 && std::string_view(argv[1]) == "--help")
		{
			auto LineEnd = [&] (bool def) { return def ? " (default)\n" : "\n"; };
			auto LineEndDefWithFlag = [&] (RunFlags flag) { return LineEnd(HasFlag(runConfig.flags, flag)); };
			auto LineEndDefWithoutFlag = [&] (RunFlags flag) { return LineEnd(!HasFlag(runConfig.flags, flag)); };
			
			std::cout <<
				"EG Arguments: \n"
				"  --gl     Force rendering with OpenGL" << LineEnd(runConfig.graphicsAPI == eg::GraphicsAPI::OpenGL) <<
				"  --vk     Force rendering with Vulkan" << LineEnd(runConfig.graphicsAPI == eg::GraphicsAPI::Vulkan) <<
				"  --igpu   Prefer integrated GPU" << LineEndDefWithFlag(RunFlags::PreferIntegratedGPU) <<
				"  --dgpu   Prefer dedicated GPU" << LineEndDefWithoutFlag(RunFlags::PreferIntegratedGPU) <<
				"  --eap    Create asset package" << LineEndDefWithFlag(RunFlags::CreateAssetPackage) <<
				"  --dev    Run in dev mode" << LineEndDefWithFlag(RunFlags::DevMode) <<
				"  --nodev  Do not run in dev mode" << LineEndDefWithoutFlag(RunFlags::DevMode) <<
				"  --vs     Enable vertical sync" << LineEndDefWithFlag(RunFlags::VSync) <<
				"  --novs   Disable vertical sync" << LineEndDefWithoutFlag(RunFlags::VSync) <<
				std::flush;
			std::exit(0);
		}
		
		for (int i = 1; i < argc; i++)
		{
			std::string_view arg = argv[i];
			if (arg == "--gl")
				runConfig.graphicsAPI = eg::GraphicsAPI::OpenGL;
			else if (arg == "--vk")
				runConfig.graphicsAPI = eg::GraphicsAPI::Vulkan;
			else if (arg == "--igpu")
				runConfig.flags |= RunFlags::PreferIntegratedGPU;
			else if (arg == "--dgpu")
				runConfig.flags &= ~RunFlags::PreferIntegratedGPU;
			else if (arg == "--eap")
				runConfig.flags |= RunFlags::CreateAssetPackage;
			else if (arg == "--dev")
				runConfig.flags |= RunFlags::DevMode;
			else if (arg == "--nodev")
				runConfig.flags &= ~RunFlags::DevMode;
			else if (arg == "--vs")
				runConfig.flags |= RunFlags::VSync;
			else if (arg == "--novs")
				runConfig.flags &= ~RunFlags::VSync;
			else
				std::cerr << "Unknown command line argument '" << arg << "'.";
		}
	}
	
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
		
#ifndef __EMSCRIPTEN__
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Runtime Error", message.c_str(), nullptr);
#endif
		
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
	
	bool TriangleContainsPoint(const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3, const glm::vec3& p)
	{
		glm::vec3 e10 = v2 - v1;
		glm::vec3 e20 = v3 - v1;
		
		float a = glm::dot(e10, e10);
		float b = glm::dot(e10, e20);
		float c = glm::dot(e20, e20);
		float ac_bb = (a * c) - (b * b);
		
		glm::vec3 vp = p - v1;
		
		float d = glm::dot(vp, e10);
		float e = glm::dot(vp, e20);
		float x = (d * c) - (e * b);
		float y = (e * a) - (d * b);
		float z = x + y - ac_bb;
		
		return ((reinterpret_cast<uint32_t&>(z)& ~(reinterpret_cast<uint32_t&>(x) | reinterpret_cast<uint32_t&>(y))) & 0x80000000);
	}
	
	std::string CanonicalPath(std::string_view path)
	{
		std::vector<std::string_view> parts;
		IterateStringParts(path, '/', [&] (std::string_view part)
		{
			if (part == ".." && !parts.empty())
			{
				parts.pop_back();
			}
			else if (part != ".")
			{
				parts.push_back(part);
			}
		});
		
		if (parts.empty())
			return {};
		
		std::ostringstream outStream;
		outStream << parts[0];
		for (size_t i = 1; i < parts.size(); i++)
			outStream << "/" << parts[i];
		return outStream.str();
	}
}
