#pragma once

#include "AbstractionHL.hpp"

namespace eg
{
class FramebufferLazyPipeline
{
public:
	FramebufferLazyPipeline() = default;
	explicit FramebufferLazyPipeline(const GraphicsPipelineCreateInfo& createInfo) : m_createInfo(createInfo) {}

	void BindPipeline(const ColorAndDepthFormat& framebufferFormat);

	void DestroyPipelines() { m_pipelines.clear(); }

private:
	GraphicsPipelineCreateInfo m_createInfo;
	std::vector<std::pair<ColorAndDepthFormat, Pipeline>> m_pipelines;
};
} // namespace eg
