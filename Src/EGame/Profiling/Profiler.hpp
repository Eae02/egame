#pragma once

#include "../API.hpp"
#include "../Graphics/AbstractionHL.hpp"
#include "ProfilingResults.hpp"

namespace eg
{
class Profiler;

class EG_API CPUTimer
{
public:
	CPUTimer() : m_profiler(nullptr), m_index(-1) {}

	~CPUTimer() { Stop(); }

	CPUTimer(CPUTimer&& other) noexcept : m_profiler(other.m_profiler), m_index(other.m_index)
	{
		other.m_profiler = nullptr;
	}

	CPUTimer& operator=(CPUTimer&& other) noexcept
	{
		Stop();
		m_profiler = other.m_profiler;
		m_index = other.m_index;
		other.m_profiler = nullptr;
		return *this;
	}

	CPUTimer(const CPUTimer& other) = delete;
	CPUTimer& operator=(const CPUTimer& other) = delete;

	void Stop();

private:
	friend class Profiler;

	CPUTimer(Profiler* profiler, int index) : m_profiler(profiler), m_index(index) {}

	Profiler* m_profiler;
	int m_index;
};

class EG_API GPUTimer
{
public:
	GPUTimer() : m_profiler(nullptr), m_index(-1) {}

	~GPUTimer() { Stop(); }

	GPUTimer(GPUTimer&& other) noexcept : m_profiler(other.m_profiler), m_index(other.m_index)
	{
		other.m_profiler = nullptr;
	}

	GPUTimer& operator=(GPUTimer&& other) noexcept
	{
		Stop();
		m_profiler = other.m_profiler;
		m_index = other.m_index;
		other.m_profiler = nullptr;
		return *this;
	}

	GPUTimer(const GPUTimer& other) = delete;
	GPUTimer& operator=(const GPUTimer& other) = delete;

	void Stop();

private:
	friend class Profiler;

	GPUTimer(Profiler* profiler, int index) : m_profiler(profiler), m_index(index) {}

	Profiler* m_profiler;
	int m_index;
};

class EG_API Profiler
{
public:
	Profiler();

	Profiler(const Profiler& other) = delete;
	Profiler(Profiler&& other) = default;
	Profiler& operator=(const Profiler& other) = delete;
	Profiler& operator=(Profiler&& other) = default;

	void Reset();
	void OnFrameBegin();

	CPUTimer StartCPUTimer(std::string name);

	GPUTimer StartGPUTimer(std::string name);

	std::optional<ProfilingResults> GetResults();

	static Profiler* current;

private:
	template <typename T, typename GetTimeCB>
	static int InitTimerTreeRec(
		std::vector<ProfilingResults::Timer>& timersOut, std::vector<T>& timersIn, size_t rootTimer, int depth,
		GetTimeCB getTime);

	template <typename T, typename GetTimeCB>
	static void InitTimerTree(
		std::vector<ProfilingResults::Timer>& timersOut, std::vector<T>& timersIn, GetTimeCB getTime);

	friend class CPUTimer;
	friend class GPUTimer;

	static constexpr uint32_t QUERIES_PER_POOL = 64;
	static constexpr uint32_t TIMERS_PER_POOL = QUERIES_PER_POOL / 2;

	struct TimerEntry
	{
		std::string name;
		int parentTimer;
	};

	struct CPUTimerEntry : TimerEntry
	{
		int64_t startTime;
		int64_t endTime;
	};

	std::vector<CPUTimerEntry> m_cpuTimers;
	int m_lastCPUTimer = -1;

	std::vector<QueryPool> m_queryPools;
	size_t m_numQueryPoolsUsed = 0;
	bool m_addQueryPool = false;

	std::vector<TimerEntry> m_gpuTimers;
	int m_lastGPUTimer = -1;
};

template <typename T>
inline T StartTimer(std::string name);

template <>
inline CPUTimer StartTimer(std::string name)
{
	if (Profiler::current != nullptr)
		return Profiler::current->StartCPUTimer(std::move(name));
	return CPUTimer();
}

template <>
inline GPUTimer StartTimer(std::string name)
{
	if (Profiler::current != nullptr)
		return Profiler::current->StartGPUTimer(std::move(name));
	return GPUTimer();
}

inline CPUTimer StartCPUTimer(std::string name)
{
	return StartTimer<CPUTimer>(std::move(name));
}

inline GPUTimer StartGPUTimer(std::string name)
{
	return StartTimer<GPUTimer>(std::move(name));
}

template <typename TimerTP>
class MultiStageTimer
{
public:
	MultiStageTimer() = default;

	void StartStage(std::string name)
	{
		m_timer.Stop();
		m_timer = StartTimer<TimerTP>(std::move(name));
	}

	void Stop() { m_timer.Stop(); }

private:
	TimerTP m_timer;
};

using MultiStageGPUTimer = MultiStageTimer<GPUTimer>;
using MultiStageCPUTimer = MultiStageTimer<CPUTimer>;
} // namespace eg
