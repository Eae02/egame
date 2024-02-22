#include "MetalCommandContext.hpp"
#include "MetalPipeline.hpp"

#include <Metal/MTLRenderCommandEncoder.hpp>

namespace eg::graphics_api::mtl
{
MetalCommandContext MetalCommandContext::main;

void MetalCommandContext::Commit()
{
	m_commandBuffer->commit();
}

void MetalCommandContext::BeginRenderPass(const MTL::RenderPassDescriptor& descriptor)
{
	EG_ASSERT(m_renderEncoder == nullptr);
	FlushComputeCommands();
	FlushBlitCommands();

	m_renderEncoder = m_commandBuffer->renderCommandEncoder(&descriptor);

	m_renderState = {};

	boundGraphicsPipelineState = nullptr;
}

void MetalCommandContext::EndRenderPass()
{
	EG_ASSERT(m_renderEncoder != nullptr);
	m_renderEncoder->endEncoding();
	m_renderEncoder = nullptr;
}

void MetalCommandContext::FlushBlitCommands()
{
	if (m_blitEncoder != nullptr)
	{
		m_blitEncoder->endEncoding();
		m_blitEncoder = nullptr;
	}
}

void MetalCommandContext::FlushComputeCommands()
{
	if (m_computeEncoder != nullptr)
	{
		m_computeEncoder->endEncoding();
		m_computeEncoder = nullptr;
	}
}

void MetalCommandContext::FlushDrawState()
{
	if (m_renderState.scissorRectChanged)
	{
		m_renderEncoder->setScissorRect(m_renderState.scissorRect);
		m_renderState.scissorRectChanged = false;
	}

	if (m_renderState.viewportChanged)
	{
		m_renderEncoder->setViewport(m_renderState.viewport);
		m_renderState.viewportChanged = false;
	}

	if (m_renderState.cullModeChanged)
	{
		m_renderEncoder->setCullMode(m_renderState.cullMode);
		m_renderState.cullModeChanged = false;
	}

	if (m_renderState.triangleFillModeChanged)
	{
		m_renderEncoder->setTriangleFillMode(m_renderState.triangleFillMode);
		m_renderState.triangleFillModeChanged = false;
	}

	if (pushConstantsChanged)
	{
		const StageBindingsTable* vsBindingsTable = boundGraphicsPipelineState->bindingsTableVS.get();
		const StageBindingsTable* fsBindingsTable = boundGraphicsPipelineState->bindingsTableFS.get();

		if (vsBindingsTable != nullptr && vsBindingsTable->pushConstantsBinding != -1)
		{
			m_renderEncoder->setVertexBytes(
				pushConstantData.data(), pushConstantData.size(), vsBindingsTable->pushConstantsBinding);
		}
		if (fsBindingsTable != nullptr && fsBindingsTable->pushConstantsBinding != -1)
		{
			m_renderEncoder->setFragmentBytes(
				pushConstantData.data(), pushConstantData.size(), fsBindingsTable->pushConstantsBinding);
		}

		pushConstantsChanged = false;
	}
}

void MetalCommandContext::BindTexture(MTL::Texture* texture, uint32_t set, uint32_t binding)
{
	if (m_renderEncoder != nullptr)
	{
		if (auto location = boundGraphicsPipelineState->GetResourceMetalIndexVS(set, binding))
			m_renderEncoder->setVertexTexture(texture, *location);
		if (auto location = boundGraphicsPipelineState->GetResourceMetalIndexFS(set, binding))
			m_renderEncoder->setFragmentTexture(texture, *location);
	}
}

void MetalCommandContext::BindSampler(MTL::SamplerState* sampler, uint32_t set, uint32_t binding)
{
	if (m_renderEncoder != nullptr)
	{
		if (auto location = boundGraphicsPipelineState->GetResourceMetalIndexVS(set, binding))
			m_renderEncoder->setVertexSamplerState(sampler, *location);
		if (auto location = boundGraphicsPipelineState->GetResourceMetalIndexFS(set, binding))
			m_renderEncoder->setFragmentSamplerState(sampler, *location);
	}
}

void MetalCommandContext::BindBuffer(MTL::Buffer* buffer, uint64_t offset, uint32_t set, uint32_t binding)
{
	if (m_renderEncoder != nullptr)
	{
		if (auto location = boundGraphicsPipelineState->GetResourceMetalIndexVS(set, binding))
			m_renderEncoder->setVertexBuffer(buffer, offset, *location);
		if (auto location = boundGraphicsPipelineState->GetResourceMetalIndexFS(set, binding))
			m_renderEncoder->setFragmentBuffer(buffer, offset, *location);
	}
}

MTL::BlitCommandEncoder& MetalCommandContext::GetBlitCmdEncoder()
{
	EG_ASSERT(m_renderEncoder == nullptr);

	if (m_blitEncoder == nullptr)
	{
		m_blitEncoder = m_commandBuffer->blitCommandEncoder();
	}

	return *m_blitEncoder;
}

void MetalCommandContext::SetViewport(const MTL::Viewport& viewport)
{
	if (std::memcmp(&m_renderState.viewport, &viewport, sizeof(MTL::Viewport)))
	{
		m_renderState.viewport = viewport;
		m_renderState.viewportChanged = true;
	}
}

void MetalCommandContext::SetScissor(const MTL::ScissorRect& scissorRect)
{
	if (std::memcmp(&m_renderState.scissorRect, &scissorRect, sizeof(MTL::ScissorRect)))
	{
		m_renderState.scissorRect = scissorRect;
		m_renderState.scissorRectChanged = true;
	}
}

void MetalCommandContext::SetCullMode(MTL::CullMode cullMode)
{
	if (cullMode != m_renderState.cullMode)
	{
		m_renderState.cullMode = cullMode;
		m_renderState.cullModeChanged = true;
	}
}

void MetalCommandContext::SetTriangleFillMode(MTL::TriangleFillMode fillMode)
{
	if (m_renderState.triangleFillMode != fillMode)
	{
		m_renderState.triangleFillMode = fillMode;
		m_renderState.triangleFillModeChanged = true;
	}
}
void MetalCommandContext::SetFrontFaceCCW(bool frontFaceCCW)
{
	if (frontFaceCCW != m_renderState.currentFrontFaceCCW)
	{
		m_renderEncoder->setFrontFacingWinding(frontFaceCCW ? MTL::WindingCounterClockwise : MTL::WindingClockwise);
		m_renderState.currentFrontFaceCCW = frontFaceCCW;
	}
}

void MetalCommandContext::SetEnableDepthClamp(bool enableDepthClamp)
{
	if (enableDepthClamp != m_renderState.currentEnableDepthClamp)
	{
		m_renderEncoder->setDepthClipMode(enableDepthClamp ? MTL::DepthClipModeClamp : MTL::DepthClipModeClip);
		m_renderState.currentEnableDepthClamp = enableDepthClamp;
	}
}

void MetalCommandContext::SetBlendColor(const std::array<float, 4>& blendColor)
{
	if (blendColor != m_renderState.currentBlendColor)
	{
		m_renderEncoder->setBlendColor(blendColor[0], blendColor[1], blendColor[2], blendColor[3]);
		m_renderState.currentBlendColor = blendColor;
	}
}
} // namespace eg::graphics_api::mtl
