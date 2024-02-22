#if defined(__linux__) || defined(__APPLE__)

#include "../String.hpp"
#include "../Utils.hpp"
#include "Debug.hpp"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include <cxxabi.h>
#include <execinfo.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

namespace eg
{
static char ADDR2LINE_PATH[] = "/usr/bin/addr2line";

static std::optional<std::string> RunAddr2Line(const std::string& binaryPath, const std::string& symbol)
{
	int link[2];
	if (pipe(link) == -1)
		return std::nullopt;

	pid_t pid = fork();
	if (pid == -1)
		return std::nullopt;

	if (pid == 0)
	{
		dup2(link[1], STDOUT_FILENO);
		close(link[0]);
		close(link[1]);
		char* args[] = {
			ADDR2LINE_PATH,
			const_cast<char*>("-e"),
			const_cast<char*>(binaryPath.c_str()),
			const_cast<char*>("-Cifpa"),
			const_cast<char*>(symbol.data()),
			nullptr,
		};
		execv(ADDR2LINE_PATH, args);
		EG_UNREACHABLE
	}
	else
	{
		close(link[1]);

		std::string result;
		char readBuffer[1024];
		while (true)
		{
			ssize_t bytesRead = read(link[0], readBuffer, sizeof(readBuffer));
			if (bytesRead <= 0)
				break;
			result += std::string_view(readBuffer, bytesRead);
		}
		wait(nullptr);

		return result;
	}
}

std::vector<std::string> GetStackTrace()
{
	std::array<void*, 128> trace = {};
	size_t traceSize = backtrace(trace.data(), trace.size());
	if (traceSize == 0)
		return {};

	char** traceSybmols = backtrace_symbols(trace.data(), ToInt(traceSize));
	if (traceSybmols == nullptr)
		return {};

	bool hasAddr2Line = access(ADDR2LINE_PATH, X_OK) == 0;

	std::vector<std::string> result(traceSize);
	for (size_t i = 0; i < traceSize; i++)
	{
		result[i] = traceSybmols[i];

		if (!hasAddr2Line)
			continue;

		size_t pathEnd = 0;
		while (pathEnd < result[i].size() && !isspace(result[i][pathEnd]) && result[i][pathEnd] != '(')
			pathEnd++;

		if (pathEnd >= result[i].size())
			continue;

		char symbolDelim = result[i][pathEnd];
		size_t symbolBegin = pathEnd + 1;

		char symbolEndDelim = symbolDelim == '(' ? ')' : ' ';
		size_t symbolEnd = result[i].find(symbolEndDelim, symbolBegin);
		std::string symbol = result[i].substr(
			symbolBegin, symbolEnd == std::string::npos ? std::string::npos : (symbolEnd - symbolBegin));

		std::string path = result[i].substr(0, pathEnd);

		if (std::optional<std::string> demangled = RunAddr2Line(path, symbol))
		{
			if (demangled->back() == '\n')
				demangled->pop_back();
			result[i] = Concat({ result[i], " = ", *demangled });
		}
	}

	free(traceSybmols);
	return result;
}
} // namespace eg

#endif
