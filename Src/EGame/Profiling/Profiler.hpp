#pragma once

#include "ProfilingResults.hpp"
#include "../API.hpp"
#include "../Utils.hpp"
#include "../Graphics/AbstractionHL.hpp"

namespace eg
{
	class Profiler;
	
	class EG_API CPUTimer
	{
	public:
		CPUTimer()
			: m_profiler(nullptr), m_index(-1) { }
		
		~CPUTimer()
		{
			Stop();
		}
		
		void Stop();
		
	private:
		friend class Profiler;
		
		CPUTimer(Profiler* profiler, int index)
			: m_profiler(profiler), m_index(index) { }
		
		Profiler* m_profiler;
		int m_index;
	};
	
	class EG_API GPUTimer
	{
	public:
		GPUTimer()
			: m_profiler(nullptr), m_index(-1) { }
		
		~GPUTimer()
		{
			Stop();
		}
		
		void Stop();
		
	private:
		friend class Profiler;
		
		GPUTimer(Profiler* profiler, int index)
			: m_profiler(profiler), m_index(index) { }
		
		Profiler* m_profiler;
		int m_index;
	};
	
	class EG_API Profiler
	{
	public:
		Profiler() = default;
		
		void Reset();
		
		CPUTimer StartCPUTimer(std::string name);
		
		GPUTimer StartGPUTimer(std::string name);
		
		std::optional<ProfilingResults> GetResults();
		
		static Profiler* current;
		
	private:
		template <typename T, typename GetTimeCB>
		static int InitTimerTreeRec(std::vector<ProfilingResults::Timer>& timersOut, std::vector<T>& timersIn,
			size_t rootTimer, int depth, GetTimeCB getTime);
		
		template <typename T, typename GetTimeCB>
		static void InitTimerTree(std::vector<ProfilingResults::Timer>& timersOut, std::vector<T>& timersIn,
			GetTimeCB getTime);
		
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
		
		std::vector<TimerEntry> m_gpuTimers;
		int m_lastGPUTimer = -1;
	};
	
	inline CPUTimer StartCPUTimer(std::string name)
	{
		if (Profiler::current != nullptr)
			return Profiler::current->StartCPUTimer(std::move(name));
		return CPUTimer();
	}
	
	inline GPUTimer StartGPUTimer(std::string name)
	{
		if (Profiler::current != nullptr)
			return Profiler::current->StartGPUTimer(std::move(name));
		return GPUTimer();
	}
}
