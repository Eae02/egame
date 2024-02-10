#pragma once

#include "../API.hpp"
#include "../Utils.hpp"

#include <span>
#include <string>
#include <vector>

namespace eg
{
class EG_API ProfilingResults
{
private:
	struct Timer
	{
		float timeNS;
		int numChildren;
		int totalChildren;
		int depth;
		std::string name;
	};

public:
	class TimerCursor
	{
	public:
		bool AtEnd() const { return m_index >= ToInt(m_timers.size()); }

		void Step() { m_index++; }

		void StepOver() { m_index += m_timers[m_index].totalChildren; }

		const std::string& CurrentName() const { return m_timers[m_index].name; }
		
		std::string_view CurrentNameOrEmpty() const { return AtEnd() ? std::string_view() : m_timers[m_index].name; }

		float CurrentValue() const { return m_timers[m_index].timeNS; }

		int CurrentDepth() const { return m_timers[m_index].depth; }

	private:
		friend class ProfilingResults;

		explicit TimerCursor(std::span<const Timer> timers) : m_timers(timers), m_index(0) {}

		std::span<const Timer> m_timers;
		int m_index;
	};

	ProfilingResults() = default;

	void Write(std::ostream& stream) const;

	TimerCursor GetCPUTimerCursor() const { return TimerCursor(m_cpuTimers); }

	TimerCursor GetGPUTimerCursor() const { return TimerCursor(m_gpuTimers); }

private:
	friend class TimerCursor;
	friend class Profiler;

	std::vector<Timer> m_cpuTimers;
	std::vector<Timer> m_gpuTimers;
};
} // namespace eg
