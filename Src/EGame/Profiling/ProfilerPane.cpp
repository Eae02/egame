#include "ProfilerPane.hpp"
#include "../Graphics/SpriteBatch.hpp"
#include "../Graphics/SpriteFont.hpp"

namespace eg
{
	ProfilerPane::ProfilerPane()
	{
		
	}
	
	void ProfilerPane::AddFrameResult(ProfilingResults results)
	{
		size_t historyIndex = m_historyPos % RESULT_HISTORY_LEN;
		
		auto ProcessTimers = [&] (ProfilingResults::TimerCursor cursor, bool isGPU)
		{
			while (!cursor.AtEnd())
			{
				int histIndex = FindTimerHistory(cursor.CurrentName(), isGPU);
				if (histIndex == -1)
				{
					histIndex = m_timerHistories.size();
					m_timerHistories.emplace_back(cursor.CurrentName(), isGPU);
				}
				
				m_timerHistories[histIndex].history[historyIndex] = cursor.CurrentValue();
				
				cursor.Step();
			}
		};
		
		ProcessTimers(results.GetCPUTimerCursor(), false);
		ProcessTimers(results.GetGPUTimerCursor(), true);
		
		m_lastResult = std::move(results);
		m_historyPos++;
	}
	
	void ProfilerPane::Draw(SpriteBatch& spriteBatch, int screenWidth, int screenHeight)
	{
		if (!visible)
			return;
		
		float minX = screenWidth * 0.75f;
		float minY = screenHeight * 0.25f;
		float maxY = screenHeight * 0.75f;
		
		eg::Rectangle paneRect(minX, minY, screenWidth - minX, maxY - minY);
		spriteBatch.DrawRect(paneRect, ColorLin(ColorSRGB(0.2f, 0.2f, 0.25f, 0.75f)));
		
		const float PADDING = 10;
		const float INDENT = 15;
		
		const SpriteFont& font = SpriteFont::DevFont();
		
		float y = maxY - PADDING;
		auto StepY = [&] (float lineCount)
		{
			y = std::round(y - font.LineHeight() * lineCount);
		};
		StepY(1);
		
		auto DrawTimers = [&] (ProfilingResults::TimerCursor cursor)
		{
			while (!cursor.AtEnd())
			{
				float labelX = minX + PADDING + INDENT * cursor.CurrentDepth() + 5.0f;
				spriteBatch.DrawText(font, cursor.CurrentName(), glm::vec2(labelX, y), eg::ColorLin(1, 1, 1, 0.8f));
				
				char valueBuffer[40];
				snprintf(valueBuffer, sizeof(valueBuffer), "%.2fms", cursor.CurrentValue() * 1E-6f);
				
				glm::vec2 valueExt = font.GetTextExtents(valueBuffer);
				
				spriteBatch.DrawText(font, valueBuffer, glm::vec2(screenWidth - valueExt.x - PADDING, y),
					eg::ColorLin(1, 1, 1, 1.0f));
				
				cursor.Step();
				
				StepY(1.1f);
			}
		};
		
		spriteBatch.DrawText(font, "CPU Timers:", glm::vec2(minX + PADDING, y), eg::ColorLin(1, 1, 1, 1));
		StepY(1.2f);
		DrawTimers(m_lastResult.GetCPUTimerCursor());
		
		StepY(0.5f);
		
		spriteBatch.DrawText(font, "GPU Timers:", glm::vec2(minX + PADDING, y), eg::ColorLin(1, 1, 1, 1));
		StepY(1.2f);
		DrawTimers(m_lastResult.GetGPUTimerCursor());
	}
	
	int ProfilerPane::FindTimerHistory(std::string_view name, bool isGPU) const
	{
		const uint32_t nameHash = HashFNV1a32(name);
		for (int i = 0; i < (int)m_timerHistories.size(); i++)
		{
			if (m_timerHistories[i].isGPU == isGPU && m_timerHistories[i].nameHash == nameHash)
				return i;
		}
		return -1;
	}
	
	ProfilerPane::TimerHistory::TimerHistory(std::string_view name, bool _isGPU)
		: nameHash(HashFNV1a32(name)), isGPU(_isGPU), history(RESULT_HISTORY_LEN, 0.0f) { }
}
