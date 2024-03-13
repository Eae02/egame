#pragma once

#include "API.hpp"
#include "Alloc/LinearAllocator.hpp"

#include <mutex>
#include <thread>

namespace eg
{
namespace detail
{
struct MTIBase
{
	virtual void Invoke() = 0;

	MTIBase* next = nullptr;
};

template <typename T>
struct MTI : MTIBase
{
	T callback;

	explicit MTI(T&& _callback) : callback(_callback) {}

	void Invoke() override { callback(); }
};

EG_API extern std::mutex mutexMTI;
EG_API extern LinearAllocator allocMTI;
EG_API extern MTIBase* firstMTI;
EG_API extern MTIBase* lastMTI;
EG_API extern std::thread::id mainThreadId;
} // namespace detail

template <typename CallbackTp>
void MainThreadInvoke(CallbackTp&& callback)
{
	if (std::this_thread::get_id() == detail::mainThreadId)
	{
		callback();
		return;
	}

	std::lock_guard<std::mutex> lock(detail::mutexMTI);

	auto* mti = detail::allocMTI.New<detail::MTI<CallbackTp>>(std::forward<CallbackTp>(callback));
	if (detail::lastMTI == nullptr)
		detail::firstMTI = detail::lastMTI = mti;
	else
	{
		detail::lastMTI->next = mti;
		detail::lastMTI = mti;
	}
}
} // namespace eg
