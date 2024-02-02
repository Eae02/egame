#pragma once

#include "ProfilingResults.hpp"

#include <memory>

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
} // namespace eg
