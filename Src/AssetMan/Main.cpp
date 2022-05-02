#include <iostream>
#include <fstream>

#include "ParseArguments.hpp"
#include "InfoOutput.hpp"
#include "../EGame/Alloc/LinearAllocator.hpp"
#include "../EGame/Assets/EAPFile.hpp"

int main(int argc, char** argv)
{
	if (argc <= 1)
	{
		std::cout << "expected more arguments" << std::endl;
		return 1;
	}
	
	ParsedArguments parsedArguments = ParseArguments(argc, argv);
	
	std::ifstream inStream(std::string(parsedArguments.inputFileName), std::ios::binary);
	if (!inStream)
	{
		std::cout << "error opening file for reading: '" << parsedArguments.inputFileName << "'" << std::endl;
		return 1;
	}
	
	eg::LinearAllocator allocator;
	allocator.disableMultiPoolWarning = true;
	auto readResult = eg::ReadEAPFile(inStream, allocator);
	if (!readResult)
	{
		std::cout << "error reading eap from '" << parsedArguments.inputFileName << "', maybe the file is corrupt" << std::endl;
		return 1;
	}
	
	inStream.close();
	
	if (readResult->empty())
	{
		std::cout << "file ok, but contains no assets" << std::endl;
		return 0;
	}
	
	bool operationPerformed = false;
	if (parsedArguments.writeList)
	{
		WriteListOutput(*readResult);
		operationPerformed = true;
	}
	
	if (parsedArguments.removeByName.empty())
	{
		if (!operationPerformed)
			std::cout << "file ok, no operation performed" << std::endl;
		return 1;
	}
}
