#include "ProfilerPane.hpp"
#include "../Core.hpp"
#include "../Graphics/SpriteBatch.hpp"
#include "../Graphics/SpriteFont.hpp"
#include "../Hash.hpp"
#include "Memory.hpp"

namespace eg
{
std::unique_ptr<ProfilerPane> ProfilerPane::s_instance;

ProfilerPane::ProfilerPane() {}

size_t ProfilerPane::TimerReference::Hash() const
{
	return std::hash<std::string_view>()(name) ^ static_cast<size_t>(isGPU);
}

void ProfilerPane::AddFrameResult(ProfilingResults results)
{
	auto ProcessTimers = [&](ProfilingResults::TimerCursor cursor, bool isGPU)
	{
		while (!cursor.AtEnd())
		{
			TimerReference timerRef = { cursor.CurrentName(), isGPU };

			TimerHistory& timerHistory = m_timerHistories.emplace(timerRef.Hash(), TimerHistory()).first->second;

			float value = cursor.CurrentValue();

			if (timerHistory.numValues < RESULT_HISTORY_LEN)
				timerHistory.numValues++;
			else
				timerHistory.historySum -= timerHistory.history[m_nextHistoryPos];
			timerHistory.historySum += value;

			timerHistory.history[m_nextHistoryPos] = value;

			cursor.Step();
		}
	};

	ProcessTimers(results.GetCPUTimerCursor(), false);
	ProcessTimers(results.GetGPUTimerCursor(), true);

	m_lastResult = std::move(results);
	m_hasAnyResults = true;

	m_nextHistoryPos++;
	if (m_nextHistoryPos == RESULT_HISTORY_LEN)
		m_nextHistoryPos = 0;
}

void ProfilerPane::Draw(SpriteBatch& spriteBatch, int screenWidth, int screenHeight)
{
	if (!visible || !m_hasAnyResults)
		return;

	float paneWidth = static_cast<float>(screenWidth) * 0.3f;
	float minX = static_cast<float>(screenWidth) - paneWidth;

	eg::Rectangle paneRect(minX, 0.0f, paneWidth, static_cast<float>(screenHeight));
	spriteBatch.DrawRect(paneRect, ColorLin(ColorSRGB(0.1f, 0.1f, 0.15f, 0.9f)));
	spriteBatch.PushScissorF(paneRect.x, paneRect.y, paneRect.w, paneRect.h);

	const SpriteFont& font = SpriteFont::DevFont();

	const float PADDING_X = 3.0f * DisplayScaleFactor();

	const float measurementsWidth = font.SpaceAdvance() * 12 + 2.0f * PADDING_X;
	const float labelsWidth = paneWidth * 0.4f;
	const float timeBarWidth = paneRect.w - labelsWidth - measurementsWidth;
	const float barRectLeftX = paneRect.x;
	const float barRectRightX = barRectLeftX + timeBarWidth;
	const float measurementsLeftX = barRectRightX + PADDING_X;

	const float TIME_BAR_HEIGHT = 10 * DisplayScaleFactor();
	const float TIME_BAR_Y_OFFSET = -1.0f * DisplayScaleFactor();
	const float INDENT = 10 * DisplayScaleFactor();

	float y = paneRect.MaxY() - 10 * DisplayScaleFactor();
	auto StepY = [&](float lineCount) { y = std::round(y - font.LineHeight() * lineCount); };
	StepY(1);

	const float DIVIDER_LINE_WIDTH = 0.5f * DisplayScaleFactor();

	const eg::ColorLin DIVIDER_COLOR = eg::ColorLin(1.0f, 1.0f, 1.0f, 0.02f);
	const eg::ColorLin BAR_COLOR_CUR = eg::ColorLin(1.0f, 0.1f, 0.1f, 1.0f);
	const eg::ColorLin BAR_COLOR_SMOOTH = eg::ColorLin(0.5f, 0.5f, 1.0f, 1.0f);

	auto DrawTimers = [&](ProfilingResults::TimerCursor cursor, bool isGPU)
	{
		std::optional<float> frameTime;
		while (!cursor.AtEnd())
		{
			float labelX = paneRect.MaxX() - labelsWidth + INDENT * static_cast<float>(cursor.CurrentDepth());
			spriteBatch.DrawText(font, cursor.CurrentName(), glm::vec2(labelX, y), eg::ColorLin(1, 1, 1, 1));

			float currentValue = cursor.CurrentValue();

			float smoothValue = currentValue;
			TimerHistory* timerHistory = FindTimerHistory({ cursor.CurrentName(), isGPU });
			if (timerHistory && timerHistory->numValues != 0)
			{
				smoothValue = timerHistory->historySum / static_cast<float>(timerHistory->numValues);
			}

			constexpr int VALUE_CHAR_LEN = 5;

			char valueBuffer[40];
			snprintf(
				valueBuffer, sizeof(valueBuffer), "%*.2f %*.2f", VALUE_CHAR_LEN, cursor.CurrentValue() * 1E-6f,
				VALUE_CHAR_LEN, smoothValue * 1E-6f);

			spriteBatch.DrawText(font, valueBuffer, glm::vec2(measurementsLeftX, y), eg::ColorLin(1, 1, 1, 1));

			if (frameTime.has_value())
			{
				float dividerY = y + TIME_BAR_HEIGHT + (font.LineHeight() - TIME_BAR_HEIGHT) / 2.0f - 1.0f;
				spriteBatch.DrawLine(
					glm::vec2(paneRect.x, dividerY), glm::vec2(paneRect.MaxX(), dividerY), DIVIDER_COLOR,
					DIVIDER_LINE_WIDTH);

				float barWidthCur = timeBarWidth * glm::clamp(cursor.CurrentValue() / *frameTime, 0.0f, 1.0f);
				eg::Rectangle barRectCur(
					barRectRightX - barWidthCur, y + TIME_BAR_Y_OFFSET, barWidthCur, TIME_BAR_HEIGHT / 2.0f);
				spriteBatch.DrawRect(barRectCur, BAR_COLOR_CUR);

				float barWidthSmooth = timeBarWidth * glm::clamp(smoothValue / *frameTime, 0.0f, 1.0f);
				eg::Rectangle barRectSmooth(
					barRectRightX - barWidthSmooth, y + TIME_BAR_Y_OFFSET + TIME_BAR_HEIGHT / 2.0f, barWidthSmooth,
					TIME_BAR_HEIGHT / 2.0f);
				spriteBatch.DrawRect(barRectSmooth, BAR_COLOR_SMOOTH);
			}
			else
			{
				frameTime = smoothValue;
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
		gpuMemoryUsage = static_cast<float>(memoryStat.allocatedBytesGPU) / (1024.0f * 1024.0f);
	}

	char topTextBuffer[1024];
	snprintf(
		topTextBuffer, sizeof(topTextBuffer), "FPS: %.2f Hz\nMemory Usage (RSS): %.2f MiB\nGPU Memory Usage: %.2f MiB",
		fps, memUsage, gpuMemoryUsage);
	glm::vec2 topTextSize;
	spriteBatch.DrawTextMultiline(
		font, topTextBuffer, glm::vec2(minX + PADDING_X, y), eg::ColorLin(1, 1, 1, 1.0f), 1.0f, 0.5f, &topTextSize);

	y -= topTextSize.y;
	StepY(0.4f);

	spriteBatch.DrawText(font, "CPU Timers:", glm::vec2(minX + PADDING_X, y), eg::ColorLin(1, 1, 1, 1));
	StepY(1.2f);
	DrawTimers(m_lastResult.GetCPUTimerCursor(), false);

	StepY(0.5f);

	spriteBatch.DrawText(font, "GPU Timers:", glm::vec2(minX + PADDING_X, y), eg::ColorLin(1, 1, 1, 1));
	StepY(1.2f);
	DrawTimers(m_lastResult.GetGPUTimerCursor(), true);

	const float graphHeight = paneWidth * 0.5f;

	auto DrawGraph = [&](std::span<TimerHistory*> timers, std::span<const eg::ColorLin> colors, float graphMinY)
	{
		constexpr size_t AVG_LEN = 2;

		float maxValue = 0.0f;
		std::vector<std::vector<float>> values(timers.size());
		for (size_t t = 0; t < timers.size(); t++)
		{
			if (timers[t] == nullptr)
				continue;

			for (size_t i = m_nextHistoryPos % AVG_LEN; i + AVG_LEN <= timers[t]->numValues; i += AVG_LEN)
			{
				float value = 0.0f;
				for (size_t j = 0; j < AVG_LEN; j++)
				{
					size_t idx = (m_nextHistoryPos + RESULT_HISTORY_LEN * 2 - (i + j + 1)) % RESULT_HISTORY_LEN;
					value += timers[t]->history[idx];
				}
				value /= static_cast<float>(AVG_LEN);
				maxValue = std::max(maxValue, value);
				values[t].push_back(value);
			}
		}

		if (maxValue == 0.0f)
			return;

		const float TIME_LABELS_WIDTH = font.SpaceAdvance() * 6;
		float linesMinX = paneRect.x + TIME_LABELS_WIDTH;
		float linesSpaceWidth = paneRect.MaxX() - linesMinX;

		constexpr float INCREMENT_SIZES_MS[] = { 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f };
		constexpr int MAX_INCREMENTS = 10;
		int numIncrements = 0;
		float incrementSizeMS = 0;
		for (float incSizeMS : INCREMENT_SIZES_MS)
		{
			float incSizeNS = incSizeMS * 1E6f;
			int candidateNumIncrements = static_cast<int>(std::ceil(maxValue / incSizeNS));
			if (candidateNumIncrements <= MAX_INCREMENTS ||
			    incSizeMS == INCREMENT_SIZES_MS[std::size(INCREMENT_SIZES_MS) - 1])
			{
				maxValue = static_cast<float>(candidateNumIncrements) * incSizeNS;
				incrementSizeMS = incSizeMS;
				numIncrements = candidateNumIncrements;
				break;
			}
		}

		const float GUIDE_LINE_ALPHA = 0.05f;
		const float GUIDE_LINE_WIDTH = 0.5f * DisplayScaleFactor();
		const float GUIDE_LABEL_ALPHA = 0.1f;

		for (int i = 0; i <= numIncrements; i++)
		{
			float lineY = graphMinY + graphHeight * static_cast<float>(i) / static_cast<float>(numIncrements);
			spriteBatch.DrawLine(
				glm::vec2(linesMinX, lineY), glm::vec2(paneRect.MaxX(), lineY), eg::ColorLin(1, 1, 1, GUIDE_LINE_ALPHA),
				GUIDE_LINE_WIDTH);

			float valueMS = incrementSizeMS * static_cast<float>(i);

			char labelBuffer[40];
			snprintf(labelBuffer, sizeof(labelBuffer), incrementSizeMS >= 1.0f ? "%.0fms" : "%.1fms", valueMS);

			glm::vec2 labelExtents = font.GetTextExtents(labelBuffer);

			spriteBatch.DrawText(
				font, labelBuffer, glm::vec2(linesMinX - PADDING_X - labelExtents.x, lineY - labelExtents.y / 2.0f),
				eg::ColorLin(1, 1, 1, GUIDE_LABEL_ALPHA));
		}

		const float LINE_SIZE = 0.5f * DisplayScaleFactor();

		for (size_t t = 0; t < timers.size(); t++)
		{
			if (timers[t] == nullptr || values[t].empty())
				continue;

			auto GetYValue = [&](int i)
			{
				size_t clampedI = glm::clamp(i, 0, static_cast<int>(values[t].size()) - 1);
				return graphMinY + values[t][clampedI] / maxValue * graphHeight;
			};

			std::vector<glm::vec2> positions;

			float dx = linesSpaceWidth / static_cast<float>(RESULT_HISTORY_LEN / AVG_LEN - 1);

			for (int i = 0; i < static_cast<int>(values[t].size()); i++)
			{
				float thisY = GetYValue(i);
				float leftY = GetYValue(i + 1);
				float rightY = GetYValue(i - 1);

				glm::vec2 centerPos(paneRect.MaxX() - static_cast<float>(i) * dx, thisY);

				glm::vec2 toLeftPos = glm::normalize(glm::vec2(-dx, leftY - thisY));
				glm::vec2 toRightPos = glm::normalize(glm::vec2(dx, rightY - thisY));

				if (std::abs(toLeftPos.y) > 0.5f || std::abs(toRightPos.y) > 0.5f)
				{
					glm::vec2 leftRad = glm::vec2(toLeftPos.y, -toLeftPos.x) * LINE_SIZE;
					glm::vec2 rightRad = glm::vec2(toRightPos.y, -toRightPos.x) * LINE_SIZE;

					positions.push_back(centerPos - rightRad);
					positions.push_back(centerPos + rightRad);
					positions.push_back(centerPos + leftRad);
					positions.push_back(centerPos - leftRad);
				}
				else
				{
					positions.push_back(centerPos + glm::vec2(0.0f, LINE_SIZE));
					positions.push_back(centerPos - glm::vec2(0.0f, LINE_SIZE));
				}
			}

			std::vector<uint32_t> indices;
			for (uint32_t v = 0; v + 4 <= positions.size(); v += 2)
			{
				indices.push_back(v);
				indices.push_back(v + 1);
				indices.push_back(v + 2);
				indices.push_back(v + 2);
				indices.push_back(v + 1);
				indices.push_back(v + 3);
			}

			spriteBatch.DrawCustomShape(positions, indices, colors[t]);
		}
	};

	TimerHistory* graphFrameTimers[3] = {
		FindTimerHistory({ "Frame", false }),
		FindTimerHistory({ "GPU Sync", false }),
		FindTimerHistory({ "Frame", true }),
	};
	eg::ColorLin graphFrameColors[3] = {
		eg::ColorLin(1.0f, 0.1f, 0.1f, 1.0f),
		eg::ColorLin(0.5f, 0.5f, 1.0f, 1.0f),
		eg::ColorLin(0.1f, 1.0f, 0.1f, 1.0f),
	};
	DrawGraph(graphFrameTimers, graphFrameColors, 10.0f);

	spriteBatch.PopScissor();
}

ProfilerPane::TimerHistory* ProfilerPane::FindTimerHistory(TimerReference reference)
{
	size_t hash = reference.Hash();
	auto it = m_timerHistories.find(hash);
	return it == m_timerHistories.end() ? nullptr : &it->second;
}
} // namespace eg
