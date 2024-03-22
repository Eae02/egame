#pragma once

#include "../Abstraction.hpp"
#include "WGPU.hpp"

#include <atomic>
#include <semaphore>

namespace eg::graphics_api::webgpu
{
class AbstractPipeline;

struct Viewport
{
	float x, y, w, h;
	bool operator==(const Viewport&) const = default;
	bool operator!=(const Viewport&) const = default;
};

struct ScissorRect
{
	uint32_t x, y, w, h;
	bool operator==(const ScissorRect&) const = default;
	bool operator!=(const ScissorRect&) const = default;
};

class CommandContext
{
public:
	CommandContext() = default;

	void BeginEncode();
	void EndEncode();
	void Submit();

	static CommandContext main;

	static CommandContext& Unwrap(CommandContextHandle handle)
	{
		if (handle == nullptr)
			return main;
		return *reinterpret_cast<CommandContext*>(handle);
	}

	void BeginRenderPass(
		const WGPURenderPassDescriptor& descriptor, uint32_t framebufferWidth, uint32_t framebufferHeight);

	void SetViewport(const Viewport& viewport);
	void SetScissor(const std::optional<ScissorRect>& scissorRect);
	void SetDynamicCullMode(CullMode cullMode);

	void DynamicCullModeMarkDirty() { m_renderState.dynamiccullModeChanged = true; }

	void FlushDrawState();

	uint32_t FramebufferWidth() const { return m_framebufferWidth; }
	uint32_t FramebufferHeight() const { return m_framebufferHeight; }

	void EndComputePass();

	void AddReadbackBuffer(struct Buffer& buffer);

	WGPUCommandBuffer commandBuffer = nullptr;

	WGPUCommandEncoder encoder = nullptr;
	WGPURenderPassEncoder renderPassEncoder = nullptr;
	WGPUComputePassEncoder computePassEncoder = nullptr;

	AbstractPipeline* currentPipeline = nullptr;

private:
	struct RenderState
	{
		std::array<float, 4> currentBlendColor{};

		CullMode dynamicCullMode = CullMode::None;
		bool dynamiccullModeChanged = false;

		Viewport viewport{};
		bool viewportChanged = true;

		ScissorRect scissorRect{};
		bool scissorRectChanged = true;
	};

	uint32_t m_framebufferWidth = 0;
	uint32_t m_framebufferHeight = 0;

	RenderState m_renderState;

	std::vector<struct Buffer*> m_readbackBuffers;
};
} // namespace eg::graphics_api::webgpu
