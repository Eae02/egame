#include "MainThreadInvoke.hpp"

namespace eg
{
std::mutex detail::mainThreadInvokeMutex;
std::vector<std::function<void()>> detail::mainThreadInvokeFunctions;
std::thread::id detail::mainThreadId;

void detail::RunMainThreadInvokeCallbacks()
{
	std::lock_guard<std::mutex> lock(mainThreadInvokeMutex);
	for (const auto& func : mainThreadInvokeFunctions)
		func();
	mainThreadInvokeFunctions.clear();
}
} // namespace eg
