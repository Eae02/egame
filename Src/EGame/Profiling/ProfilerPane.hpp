#pragma once

#include "ProfilingResults.hpp"

#include <memory>
#include <unordered_map>

namespace eg
{
class EG_API ProfilerPane
{
public:
	void AddFrameResult(ProfilingResults results);

	void Draw(class SpriteBatch& spriteBatch, int screenWidth, int screenHeight);

	bool visible = false;

	static ProfilerPane* Instance() { return s_instance.get(); }

private:
	friend bool EnableProfiling(); // creates s_instance

	static std::unique_ptr<ProfilerPane> s_instance;

	ProfilerPane();

	bool m_hasAnyResults = false;
	ProfilingResults m_lastResult;

	size_t m_nextHistoryPos = 0;
	static constexpr size_t RESULT_HISTORY_LEN = 512;
	static constexpr float TIMER_RUNNING_AVERAGE_TIME_SPAN = 2.0f;

	struct TimerHistory
	{
		std::array<float, RESULT_HISTORY_LEN> history{};
		float historySum = 0.0f;
		size_t numValues = 0;
	};

	struct TimerReference
	{
		std::string_view name;
		bool isGPU;

		size_t Hash() const;
	};

	std::unordered_map<size_t, TimerHistory> m_timerHistories;

	TimerHistory* FindTimerHistory(TimerReference reference);
};
} // namespace eg
