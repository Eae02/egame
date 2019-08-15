#pragma once

#include "ProfilingResults.hpp"

namespace eg
{
	class EG_API ProfilerPane
	{
	public:
		ProfilerPane();
		
		void AddFrameResult(ProfilingResults results);
		
		void Draw(class SpriteBatch& spriteBatch, int screenWidth, int screenHeight);
		
		bool visible = false;
		
	private:
		ProfilingResults m_lastResult;
		
		size_t m_historyPos = 0;
		static constexpr size_t RESULT_HISTORY_LEN = 128;
		
		struct TimerHistory
		{
			uint32_t nameHash;
			bool isGPU;
			std::vector<float> history;
			
			TimerHistory(std::string_view _name, bool isGPU);
		};
		
		std::vector<TimerHistory> m_timerHistories;
		
		std::vector<std::string> m_timerGraphs;
		
		int FindTimerHistory(std::string_view name, bool isGPU) const;
	};
}
