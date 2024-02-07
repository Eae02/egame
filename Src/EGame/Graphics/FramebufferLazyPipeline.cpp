#include "FramebufferLazyPipeline.hpp"

namespace eg
{
void FramebufferLazyPipeline::BindPipeline(const ColorAndDepthFormat& framebufferFormat)
{
	if (const Pipeline* pipeline = LinearLookup<ColorAndDepthFormat, Pipeline>(m_pipelines, framebufferFormat))
	{
		eg::DC.BindPipeline(*pipeline);
		return;
	}

	m_createInfo.numColorAttachments = framebufferFormat.color != eg::Format::Undefined;
	m_createInfo.colorAttachmentFormats[0] = framebufferFormat.color;
	m_createInfo.depthAttachmentFormat = framebufferFormat.depth;
	Pipeline pipeline = Pipeline::Create(m_createInfo);

	eg::DC.BindPipeline(pipeline);

	m_pipelines.emplace_back(framebufferFormat, std::move(pipeline));
}
} // namespace eg
