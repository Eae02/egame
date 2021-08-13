#include "ProfilerPane.hpp"
#include "../Graphics/SpriteBatch.hpp"
#include "../Graphics/SpriteFont.hpp"
#include "Memory.hpp"

namespace eg
{
	ProfilerPane::ProfilerPane()
	{
		m_timerGraphs.push_back("");
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
					histIndex = (int)m_timerHistories.size();
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
		
		float paneWidth = std::max((float)screenWidth * 0.25f, 400.0f);
		float minX = (float)screenWidth - paneWidth;
		float minY = (float)screenHeight * 0.05f;
		float maxY = (float)screenHeight * 0.95f;
		
		eg::Rectangle paneRect(minX, minY, paneWidth, maxY - minY);
		spriteBatch.DrawRect(paneRect, ColorLin(ColorSRGB(0.2f, 0.2f, 0.25f, 0.75f)));
		spriteBatch.PushScissorF(paneRect.x, paneRect.y, paneRect.w, paneRect.h);
		
		float timeBarWidth = paneWidth * 0.25f;
		
		const float TIME_BAR_HEIGHT = 6;
		const float PADDING = 10;
		const float INDENT = 15;
		
		const SpriteFont& font = SpriteFont::DevFont();
		
		float y = maxY - PADDING;
		auto StepY = [&] (float lineCount)
		{
			y = std::round(y - font.LineHeight() * lineCount);
		};
		StepY(1);
		
		const eg::ColorLin BAR_COLOR = eg::ColorLin(1.0f, 0.25f, 0.25f, 0.8f);
		
		auto DrawTimers = [&] (ProfilingResults::TimerCursor cursor)
		{
			std::optional<float> frameTime;
			while (!cursor.AtEnd())
			{
				float labelX = minX + PADDING + INDENT * (float)cursor.CurrentDepth() + 5.0f;
				spriteBatch.DrawText(font, cursor.CurrentName(), glm::vec2(labelX, y), eg::ColorLin(1, 1, 1, 0.8f));
				
				char valueBuffer[40];
				snprintf(valueBuffer, sizeof(valueBuffer), "%.2f ms", cursor.CurrentValue() * 1E-6f);
				glm::vec2 valueExt = font.GetTextExtents(valueBuffer);
				
				spriteBatch.DrawText(font, valueBuffer, glm::vec2((float)screenWidth - valueExt.x - PADDING, y),
					eg::ColorLin(1, 1, 1, 1.0f));
				
				if (frameTime.has_value())
				{
					float barRectMaxX = (float)screenWidth - std::max(valueExt.x + PADDING, 80.0f);
					eg::Rectangle barRectBack(barRectMaxX - timeBarWidth, y + 5, timeBarWidth, TIME_BAR_HEIGHT);
					spriteBatch.DrawRect(barRectBack, BAR_COLOR.ScaleRGB(0.5f).ScaleAlpha(0.2f));
					float barWidth = timeBarWidth * glm::clamp(cursor.CurrentValue() / *frameTime, 0.0f, 1.0f);
					eg::Rectangle barRectFront(barRectMaxX - barWidth, y + 6, barWidth - 1, TIME_BAR_HEIGHT - 2);
					spriteBatch.DrawRect(barRectFront, BAR_COLOR);
				}
				else
				{
					frameTime = cursor.CurrentValue();
				}
				
				StepY(1.1f);
				
				cursor.Step();
			}
		};
		
		double fps = 1E9 / m_lastResult.GetCPUTimerCursor().CurrentValue();
		double memUsage = GetMemoryUsageRSS() / (1024.0 * 1024.0);
		
		float gpuMemoryUsage = 0;
		if (gal::GetMemoryStat)
		{
			GraphicsMemoryStat memoryStat = gal::GetMemoryStat();
			gpuMemoryUsage = (float)memoryStat.allocatedBytesGPU / (1024.0f * 1024.0f);
		}
		
		char topTextBuffer[1024];
		snprintf(topTextBuffer, sizeof(topTextBuffer),
			"FPS: %.2f Hz\nMemory Usage (RSS): %.2f MiB\nGPU Memory Usage: %.2f MiB", fps, memUsage, gpuMemoryUsage);
		glm::vec2 topTextSize;
		spriteBatch.DrawTextMultiline(font, topTextBuffer, glm::vec2(minX + PADDING, y),
		                              eg::ColorLin(1, 1, 1, 1.0f), 1.0f, 0.5f, &topTextSize);
		
		y -= topTextSize.y;
		StepY(0.4f);
		
		spriteBatch.DrawText(font, "CPU Timers:", glm::vec2(minX + PADDING, y), eg::ColorLin(1, 1, 1, 1));
		StepY(1.2f);
		DrawTimers(m_lastResult.GetCPUTimerCursor());
		
		StepY(0.5f);
		
		spriteBatch.DrawText(font, "GPU Timers:", glm::vec2(minX + PADDING, y), eg::ColorLin(1, 1, 1, 1));
		StepY(1.2f);
		DrawTimers(m_lastResult.GetGPUTimerCursor());
		
		spriteBatch.PopScissor();
		
		if (!m_timerGraphs.empty())
		{
			
		}
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
