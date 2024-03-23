#pragma once

#include "MetalMain.hpp"

#include <Metal/MTLRenderCommandEncoder.hpp>
#include <array>
#include <unordered_map>

namespace eg::graphics_api::mtl
{
class MetalCommandContext
{
public:
	explicit MetalCommandContext(MTL::CommandBuffer* commandBuffer = nullptr) : m_commandBuffer(commandBuffer) {}

	static MetalCommandContext& Unwrap(CommandContextHandle cc)
	{
		if (cc == nullptr)
			return main;
		else
			return *reinterpret_cast<MetalCommandContext*>(cc);
	}

	void Commit();

	void BeginRenderPass(const MTL::RenderPassDescriptor& descriptor);
	void EndRenderPass();

	void FlushBlitCommands();
	void FlushComputeCommands();

	void FlushDrawState();

	MTL::BlitCommandEncoder& GetBlitCmdEncoder();

	MTL::ComputeCommandEncoder& GetComputeCmdEncoder();

	MTL::RenderCommandEncoder& RenderCmdEncoder() const
	{
		EG_ASSERT(m_renderEncoder != nullptr);
		return *m_renderEncoder;
	}

	MTL::RenderCommandEncoder* GetRenderCmdEncoder() const { return m_renderEncoder; }

	void BindTexture(MTL::Texture* texture, uint32_t set, uint32_t binding);
	void BindSampler(MTL::SamplerState* sampler, uint32_t set, uint32_t binding);
	void BindBuffer(MTL::Buffer* buffer, uint64_t offset, uint32_t set, uint32_t binding);

	void SetViewport(const MTL::Viewport& viewport);
	void SetScissor(const MTL::ScissorRect& scissorRect);
	void SetCullMode(MTL::CullMode cullMode);
	void SetTriangleFillMode(MTL::TriangleFillMode fillMode);

	void SetFrontFaceCCW(bool frontFaceCCW);
	void SetEnableDepthClamp(bool enableDepthClamp);
	void SetBlendColor(const std::array<float, 4>& blendColor);

	static MetalCommandContext main;

	const struct BoundGraphicsPipelineState* boundGraphicsPipelineState = nullptr;

	const struct ComputePipeline* currentComputePipeline = nullptr;

	uint32_t boundIndexBufferOffset = 0;
	MTL::Buffer* boundIndexBuffer = nullptr;
	MTL::IndexType boundIndexType{};

	uint32_t framebufferWidth = 0;
	uint32_t framebufferHeight = 0;

	MTL::CommandBuffer* m_commandBuffer = nullptr;

private:
	std::optional<uint32_t> GetComputePipelineMetalResourceIndex(uint32_t set, uint32_t binding) const;

	MTL::RenderCommandEncoder* m_renderEncoder = nullptr;
	MTL::BlitCommandEncoder* m_blitEncoder = nullptr;
	MTL::ComputeCommandEncoder* m_computeEncoder = nullptr;

	struct RenderState
	{
		bool currentFrontFaceCCW = false;
		bool currentEnableDepthClamp = false;

		std::array<float, 4> currentBlendColor{};

		MTL::TriangleFillMode triangleFillMode = MTL::TriangleFillModeFill;
		bool triangleFillModeChanged = false;

		MTL::CullMode cullMode = MTL::CullModeNone;
		bool cullModeChanged = false;

		MTL::Viewport viewport{};
		bool viewportChanged = true;

		MTL::ScissorRect scissorRect{};
		bool scissorRectChanged = true;
	};

	RenderState m_renderState;
};
} // namespace eg::graphics_api::mtl
