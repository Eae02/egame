#pragma once

#include <string_view>
#include <vector>

struct ParsedArguments
{
	std::string_view inputFileName;
	std::string_view outputFileName;

	bool writeInfo = false;
	bool writeList = false;
	bool dryRun = false;

	std::vector<std::string_view> removeByName;
};

ParsedArguments ParseArguments(int argc, char** argv);
