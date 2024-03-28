#pragma once

#include "API.hpp"
#include "Alloc/LinearAllocator.hpp"
#include "Assert.hpp"

#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace eg
{
namespace detail
{
EG_API extern std::mutex mainThreadInvokeMutex;
EG_API extern std::vector<std::function<void()>> mainThreadInvokeFunctions;
EG_API extern std::thread::id mainThreadId;

void RunMainThreadInvokeCallbacks();
} // namespace detail

inline bool IsMainThread()
{
	return std::this_thread::get_id() == detail::mainThreadId;
}

template <typename T>
class MainThreadInvokableUnsyncronized
{
public:
	MainThreadInvokableUnsyncronized() = default;

	template <typename InitFn>
	static MainThreadInvokableUnsyncronized<T> Init(InitFn init)
	{
		MainThreadInvokableUnsyncronized<T> result;
		if (IsMainThread())
			result.m_handle = init();
		else
			result.m_initFunction = init;
		return result;
	}

	template <typename F>
	void OnMainThread(F func)
	{
		if (m_handle.has_value())
		{
			EG_DEBUG_ASSERT(IsMainThread());
			func(MTGet());
		}
		else
		{
			m_functions.emplace_back(func);
		}
	}

	const std::optional<T>& GetOpt()
	{
		if (!m_handle.has_value() && IsMainThread())
			MTGet();
		return m_handle;
	}

	const T& MTGet()
	{
		EG_DEBUG_ASSERT(IsMainThread());
		if (!m_handle.has_value())
			m_handle = m_initFunction();
		for (const auto& func : m_functions)
			func(*m_handle);
		m_functions.clear();
		return *m_handle;
	}

private:
	std::optional<T> m_handle;
	std::function<T()> m_initFunction;
	std::vector<std::function<void(T)>> m_functions;
};

template <typename CallbackTp>
void MainThreadInvoke(CallbackTp&& callback)
{
	if (IsMainThread())
	{
		callback();
		return;
	}

	std::lock_guard<std::mutex> lock(detail::mainThreadInvokeMutex);
	detail::mainThreadInvokeFunctions.emplace_back(std::move(callback));
}
} // namespace eg
