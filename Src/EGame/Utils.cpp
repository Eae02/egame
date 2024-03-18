#include "Utils.hpp"
#include "Core.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stack>

namespace eg
{
bool detail::devMode = false;

void ParseCommandLineArgs(RunConfig& runConfig, int argc, char** argv)
{
	if (argc == 2 && std::string_view(argv[1]) == "--help")
	{
		auto LineEnd = [&](bool def) { return def ? " (default)\n" : "\n"; };
		auto LineEndDefWithFlag = [&](RunFlags flag) { return LineEnd(HasFlag(runConfig.flags, flag)); };
		auto LineEndDefWithoutFlag = [&](RunFlags flag) { return LineEnd(!HasFlag(runConfig.flags, flag)); };

		// clang-format off
		std::cout <<
			"EG Arguments: \n"
			"  --gl     Force rendering with OpenGL" << LineEnd(runConfig.graphicsAPI == eg::GraphicsAPI::OpenGL) <<
			"  --vk     Force rendering with Vulkan" << LineEnd(runConfig.graphicsAPI == eg::GraphicsAPI::Vulkan) <<
			"  --webgpu Force rendering with WebGPU" << LineEnd(runConfig.graphicsAPI == eg::GraphicsAPI::WebGPU) <<
			"  --igpu   Prefer integrated GPU" << LineEndDefWithFlag(RunFlags::PreferIntegratedGPU) <<
			"  --dgpu   Prefer dedicated GPU" << LineEndDefWithoutFlag(RunFlags::PreferIntegratedGPU) <<
			"  --gles   Prefer GLES path when using OpenGL" << LineEndDefWithFlag(RunFlags::PreferGLESPath) <<
			"  --eap    Create asset package" << LineEndDefWithFlag(RunFlags::CreateAssetPackage) <<
			"  --eapf   Create asset package (faster, no compression)" << LineEndDefWithFlag(RunFlags::CreateAssetPackage | RunFlags::AssetPackageFast) <<
			"  --dev    Run in dev mode" << LineEndDefWithFlag(RunFlags::DevMode) <<
			"  --nodev  Do not run in dev mode" << LineEndDefWithoutFlag(RunFlags::DevMode) <<
			"  --vs     Enable vertical sync" << LineEndDefWithFlag(RunFlags::VSync) <<
			"  --novs   Disable vertical sync" << LineEndDefWithoutFlag(RunFlags::VSync) <<
			std::flush;
		// clang-format on
		std::exit(0);
	}

	for (int i = 1; i < argc; i++)
	{
		std::string_view arg = argv[i];
		if (arg == "--gl")
			runConfig.graphicsAPI = eg::GraphicsAPI::OpenGL;
		else if (arg == "--vk")
			runConfig.graphicsAPI = eg::GraphicsAPI::Vulkan;
		else if (arg == "--webgpu")
			runConfig.graphicsAPI = eg::GraphicsAPI::WebGPU;
		else if (arg == "--igpu")
			runConfig.flags |= RunFlags::PreferIntegratedGPU;
		else if (arg == "--dgpu")
			runConfig.flags &= ~RunFlags::PreferIntegratedGPU;
		else if (arg == "--gles")
			runConfig.flags |= RunFlags::PreferGLESPath;
		else if (arg == "--eap")
			runConfig.flags |= RunFlags::CreateAssetPackage;
		else if (arg == "--eapf")
			runConfig.flags |= RunFlags::CreateAssetPackage | RunFlags::AssetPackageFast;
		else if (arg == "--dev")
			runConfig.flags |= RunFlags::DevMode;
		else if (arg == "--nodev")
			runConfig.flags &= ~RunFlags::DevMode;
		else if (arg == "--vs")
			runConfig.flags |= RunFlags::VSync;
		else if (arg == "--novs")
			runConfig.flags &= ~RunFlags::VSync;
	}
}

std::string ReadableBytesSize(uint64_t size)
{
	std::ostringstream stream;
	stream << std::setprecision(3);
	if (size < 1024)
		stream << size << "B";
	else if (size < 1024 * 1024)
		stream << (static_cast<double>(size) / 1024.0) << "KiB";
	else if (size < 1024 * 1024 * 1024)
		stream << (static_cast<double>(size) / (1024.0 * 1024.0)) << "MiB";
	else
		stream << (static_cast<double>(size) / (1024.0 * 1024.0 * 1024.0)) << "GiB";
	return stream.str();
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

	return ((std::bit_cast<uint32_t>(z) & ~(std::bit_cast<uint32_t>(x) | std::bit_cast<uint32_t>(y))) & 0x80000000);
}

std::string CanonicalPath(std::string_view path)
{
	std::vector<std::string_view> parts;
	IterateStringParts(
		path, '/',
		[&](std::string_view part)
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

int64_t NanoTime()
{
	using namespace std::chrono;
	return duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
}
} // namespace eg
