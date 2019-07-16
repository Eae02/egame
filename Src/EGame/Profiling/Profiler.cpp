#include "Profiler.hpp"

namespace eg
{
	void CPUTimer::Stop()
	{
		if (m_profiler == nullptr)
			return;
		
		m_profiler->m_cpuTimers[m_index].endTime = NanoTime();
		if (m_profiler->m_lastCPUTimer == m_index)
		{
			m_profiler->m_lastCPUTimer = m_profiler->m_cpuTimers[m_index].parentTimer;
		}
		m_profiler = nullptr;
	}
	
	void GPUTimer::Stop()
	{
		if (m_profiler == nullptr)
			return;
		
		DC.WriteTimestamp(m_profiler->m_queryPools[m_index / Profiler::TIMERS_PER_POOL],
			(m_index % Profiler::TIMERS_PER_POOL) * 2 + 1);
		
		if (m_profiler->m_lastGPUTimer == m_index)
		{
			m_profiler->m_lastGPUTimer = m_profiler->m_gpuTimers[m_index].parentTimer;
		}
		m_profiler = nullptr;
	}
	
	Profiler* Profiler::current = nullptr;
	
	void Profiler::Reset()
	{
		m_cpuTimers.clear();
		m_gpuTimers.clear();
	}
	
	CPUTimer Profiler::StartCPUTimer(std::string name)
	{
		size_t index = m_cpuTimers.size();
		CPUTimerEntry& entry = m_cpuTimers.emplace_back();
		entry.startTime = NanoTime();
		entry.name = std::move(name);
		entry.parentTimer = m_lastCPUTimer;
		m_lastCPUTimer = index;
		
		return CPUTimer(this, index);
	}
	
	GPUTimer Profiler::StartGPUTimer(std::string name)
	{
		size_t index = m_gpuTimers.size();
		TimerEntry& entry = m_gpuTimers.emplace_back();
		entry.name = std::move(name);
		entry.parentTimer = m_lastGPUTimer;
		m_lastGPUTimer = index;
		
		const size_t poolIndex = index / TIMERS_PER_POOL;
		if (poolIndex >= m_queryPools.size())
		{
			m_queryPools.emplace_back(QueryType::Timestamp, QUERIES_PER_POOL);
		}
		
		DC.ResetQueries(m_queryPools[poolIndex], (index % TIMERS_PER_POOL) * 2, 2);
		
		DC.WriteTimestamp(m_queryPools[poolIndex], (index % TIMERS_PER_POOL) * 2);
		
		return GPUTimer(this, index);
	}
	
	template <typename T, typename GetTimeCB>
	int Profiler::InitTimerTreeRec(std::vector<ProfilingResults::Timer>& timersOut, std::vector<T>& timersIn,
		size_t rootTimer, int depth, GetTimeCB getTime)
	{
		size_t outIndex = timersOut.size();
		ProfilingResults::Timer& timer = timersOut.emplace_back();
		timer.numChildren = 0;
		timer.totalChildren = 0;
		timer.depth = depth;
		timer.name = std::move(timersIn[rootTimer].name);
		timer.timeNS = getTime(rootTimer);
		
		for (size_t i = 0; i < timersIn.size(); i++)
		{
			if (timersIn[i].parentTimer == (int)rootTimer)
			{
				timersOut[outIndex].totalChildren += InitTimerTreeRec(timersOut, timersIn, i, depth + 1, getTime);
				timersOut[outIndex].numChildren++;
			}
		}
		
		return timersOut[outIndex].totalChildren + 1;
	}
	
	template <typename T, typename GetTimeCB>
	void Profiler::InitTimerTree(std::vector<ProfilingResults::Timer>& timersOut, std::vector<T>& timersIn, GetTimeCB getTime)
	{
		for (size_t i = 0; i < timersIn.size(); i++)
		{
			if (timersIn[i].parentTimer == -1)
			{
				InitTimerTreeRec(timersOut, timersIn, i, 0, getTime);
			}
		}
	}
	
	std::optional<ProfilingResults> Profiler::GetResults()
	{
		//Fetches timestamps from queries
		const uint32_t numTimestamps = m_gpuTimers.size() * 2;
		const uint32_t timestampBytes = numTimestamps * sizeof(uint64_t);
		uint64_t* timestamps = reinterpret_cast<uint64_t*>(alloca(timestampBytes));
		for (size_t i = 0; i * TIMERS_PER_POOL < m_gpuTimers.size(); i++)
		{
			uint32_t firstTimestamp = i * QUERIES_PER_POOL;
			uint32_t lastTimestamp = std::min<uint32_t>(numTimestamps, (i + 1) * QUERIES_PER_POOL);
			bool avail = m_queryPools[i].GetResults(0, lastTimestamp - firstTimestamp,
				timestampBytes - firstTimestamp * sizeof(uint64_t), timestamps + firstTimestamp);
			
			if (!avail)
				return { };
		}
		
		ProfilingResults results;
		
		InitTimerTree(results.m_cpuTimers, m_cpuTimers, [&] (size_t i)
		{
			return m_cpuTimers[i].endTime - m_cpuTimers[i].startTime;
		});
		
		InitTimerTree(results.m_gpuTimers, m_gpuTimers, [&] (size_t i)
		{
			float elapsedNS = (timestamps[i * 2 + 1] - timestamps[i * 2]) * GetGraphicsDeviceInfo().timerTicksPerNS;
			return (int64_t)std::round(elapsedNS);
		});
		
		return results;
	}
}
