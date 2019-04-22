#include "ProfilingResults.hpp"

#include <iomanip>

namespace eg
{
	void WriteTimers(std::ostream& stream, ProfilingResults::TimerCursor cursor)
	{
		while (!cursor.AtEnd())
		{
			for (int i = cursor.CurrentDepth() - 1; i >= 0; i--)
			{
				stream << "  ";
			}
			
			stream << cursor.CurrentName() << " - " << std::setprecision(2) << (cursor.CurrentValue() * 1E-6f) << "ms\n";
			
			cursor.Step();
		}
	}
	
	void ProfilingResults::Write(std::ostream& stream) const
	{
		stream << "CPU Timers:\n";
		WriteTimers(stream, GetCPUTimerCursor());
		
		stream << "GPU Timers:\n";
		WriteTimers(stream, GetGPUTimerCursor());
	}
}
