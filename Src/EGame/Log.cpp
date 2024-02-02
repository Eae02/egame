#include "Log.hpp"
#include "Assert.hpp"
#include "Console.hpp"
#include "Utils.hpp"

#include <ctime>
#include <iomanip>
#include <mutex>

namespace eg
{
static const char* levelMessages[] = { "I", "W", "E" };
static const char* levelColorStrings[] = { "", "\x1b[33m", "\x1b[31m" };
static const ColorLin levelColors[] = { console::InfoColor, console::WarnColor, console::ErrorColor };

static std::mutex stdoutLogMutex;

void detail::Log(LogLevel level, const char* category, const char* format, size_t argc, const std::string* argv)
{
	std::ostringstream prefixStream;
	time_t time = std::time(nullptr);
	prefixStream << std::put_time(std::localtime(&time), "%H:%M:%S") << " [" << category << " "
				 << levelMessages[static_cast<int>(level)] << "] ";
	std::string prefixString = prefixStream.str();

	std::ostringstream messageStream;

	// Buffer used when converting integers to strings
	char buffer[4];

	if (argc > 999)
		EG_PANIC("Too many arguments, can't have more than 999.");

	while (format[0] != '\0')
	{
		const char* nextArgPos = strchr(format, '{');

		if (nextArgPos == nullptr)
		{
			messageStream << format;
			break;
		}
		else
		{
			// Writes the text leading up to the next argument
			if (nextArgPos != format)
			{
				messageStream.write(format, nextArgPos - format);
			}

			// Finds the closing argument bracket
			const char* closeBracket = strchr(nextArgPos, '}');
			if (closeBracket == nullptr)
			{
				Log(LogLevel::Error, "log", "Error in log format: Missing closing bracket.");
				return;
			}

			size_t intLength = closeBracket - nextArgPos - 1;

			// Copies the index string to the buffer
			std::copy(nextArgPos + 1, closeBracket, buffer);
			buffer[intLength] = '\0';

			// Parses the index string
			int index = atoi(buffer);
			if (index < 0 || static_cast<unsigned>(index) >= argc)
			{
				Log(LogLevel::Error, "log", "Error in log format: Argument index out of range.");
				return;
			}

			// Writes the arguments
			messageStream.write(argv[index].c_str(), argv[index].length());
			format = closeBracket + 1;
		}
	}

	std::string messageStr = messageStream.str();

	console::Writer consoleWriter;
	consoleWriter.Write(levelColors[static_cast<int>(level)].ScaleAlpha(0.75f), prefixString);
	consoleWriter.Write(levelColors[static_cast<int>(level)], messageStr);

	if (level != LogLevel::Info || DevMode())
	{
#ifdef __EMSCRIPTEN__
		std::cout << prefixString << messageStr << std::endl;
#else
		std::lock_guard<std::mutex> lock(stdoutLogMutex);
		std::cout << levelColorStrings[static_cast<int>(level)] << "\x1b[2m" << prefixString << "\x1b[0m"
				  << levelColorStrings[static_cast<int>(level)] << messageStr << "\x1b[0m" << std::endl;
#endif
	}
}
} // namespace eg
