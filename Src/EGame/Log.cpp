#include "Log.hpp"
#include "Utils.hpp"
//#include "Console.hpp"

#include <iomanip>
#include <ctime>
#include <mutex>

namespace eg
{
	const char* levelMessages[] = { "I", "W", "E" };
	const char* levelColorStrings[] = { "", "\x1b[33m", "\x1b[31m" };
	//const glm::vec4 levelColors[] = { Console::InfoColor, Console::WarnColor, Console::ErrorColor };
	
	static std::mutex stdoutLogMutex;
	
	void detail::Log(LogLevel level, const char* category, const char* format, size_t argc, const std::string* argv)
	{
		std::ostringstream stream;
		
		time_t time = std::time(nullptr);
		stream << std::put_time(std::localtime(&time), "%H:%M:%S");
		
		stream << " [" << category << " " << levelMessages[(int)level] << "] ";
		
		//Buffer used when converting integers to strings
		char buffer[4];
		
		if (argc > 999)
			EG_PANIC("Too many arguments, can't have more than 999.");
		
		while (format[0] != '\0')
		{
			const char* nextArgPos = strchr(format, '{');
			
			if (nextArgPos == nullptr)
			{
				stream << format;
				break;
			}
			else
			{
				//Writes the text leading up to the next argument
				if (nextArgPos != format)
				{
					stream.write(format, nextArgPos - format);
				}
				
				//Finds the closing argument bracket
				const char* closeBracket = strchr(nextArgPos, '}');
				if (closeBracket == nullptr)
				{
					Log(LogLevel::Error, "log", "Error in log format: Missing closing bracket.");
					return;
				}
				
				size_t intLength = closeBracket - nextArgPos - 1;
				
				//Copies the index string to the buffer
				std::copy(nextArgPos + 1, closeBracket, buffer);
				buffer[intLength] = '\0';
				
				//Parses the index string
				int index = atoi(buffer);
				if (index < 0 || static_cast<unsigned>(index) >= argc)
				{
					Log(LogLevel::Error, "log", "Error in log format: Argument index out of range.");
					return;
				}
				
				//Writes the arguments
				stream.write(argv[index].c_str(), argv[index].length());
				format = closeBracket + 1;
			}
		}
		
		std::string messageStr = stream.str();
		
		//if (Console::instance)
		//{
		//	Console::instance->Write(levelColors[(int)level], messageStr);
		//}
		
		//if (level != LogLevel::Info)
		{
			std::lock_guard<std::mutex> lock(stdoutLogMutex);
			std::cout << levelColorStrings[(int)level] << messageStr << "\x1b[0m" << std::endl;
		}
	}
}
