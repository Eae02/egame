#include "ParseArguments.hpp"

#include <iostream>
#include <string_view>
#include <unordered_map>
#include <functional>
#include <variant>

ParsedArguments ParseArguments(int argc, char** argv)
{
	ParsedArguments parsed;
	
	using ArgumentCallback = std::function<void(std::string_view)>;
	using FlagCallback = std::function<void()>;
	
	std::unordered_map<std::string_view, std::variant<ArgumentCallback, FlagCallback>> argumentHandlers;
	
	argumentHandlers["o"] = [&] (std::string_view nextArg)
	{
		parsed.outputFileName = nextArg;
	};
	argumentHandlers["r"] = [&] (std::string_view nextArg)
	{
		parsed.removeByName.emplace_back(nextArg);
	};
	argumentHandlers["i"] = [&] () { parsed.writeInfo = true; };
	argumentHandlers["l"] = [&] () { parsed.writeList = true; };
	argumentHandlers["d"] = [&] () { parsed.dryRun = true; };
	
	for (int i = 1; i < argc; i++)
	{
		if (argv[i][0] != '-')
		{
			parsed.inputFileName = argv[i];
			continue;
		}
		std::string_view argName;
		if (argv[i][1] == '-')
			argName = argv[i] + 2;
		else
			argName = argv[i] + 1;
		
		auto it = argumentHandlers.find(argName);
		if (it == argumentHandlers.end())
		{
			std::cout << "unknown argument: " << argName << std::endl;
			std::exit(1);
		}
		
		if (const ArgumentCallback* argumentCallback = std::get_if<ArgumentCallback>(&it->second))
		{
			if (i + 1 == argc)
				std::exit(1);
			(*argumentCallback)(argv[i + 1]);
			i++;
		}
		else
		{
			std::get<FlagCallback>(it->second)();
		}
	}
	
	if (parsed.inputFileName.empty())
	{
		std::cout << "no input file name specified" << std::endl;
		std::exit(1);
	}
	
	if (parsed.outputFileName.empty())
		parsed.outputFileName = parsed.inputFileName;
	
	return parsed;
}
