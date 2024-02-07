#include "../../Assert.hpp"
#include "../Abstraction.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandContext.hpp"
#include "MetalMain.hpp"
#include "MetalPipeline.hpp"
#include "MetalTexture.hpp"
#include "MetalTranslation.hpp"

namespace eg::graphics_api::mtl
{
struct FramebufferAttachment
{
	MTL::Texture* texture = nullptr;
	uint32_t level = 0;
	uint32_t slice = 0;

	static FramebufferAttachment Make(const eg::FramebufferAttachment& attachment)
	{
		Texture& texture = Texture::Unwrap(attachment.texture);
		if (attachment.subresource.numArrayLayers > 1)
		{
			return FramebufferAttachment{
				.texture =
					texture.GetTextureView(TextureViewType::SameAsTexture, attachment.subresource.AsSubresource()),
			};
		}
		else
		{
			return FramebufferAttachment{
				.texture = texture.texture,
				.level = attachment.subresource.mipLevel,
				.slice = attachment.subresource.firstArrayLayer,
			};
		}
	}

	void InitDescriptor(MTL::RenderPassAttachmentDescriptor& descriptor) const
	{
		descriptor.setTexture(texture);
		descriptor.setLevel(level);
		descriptor.setSlice(slice);
	}
};

struct Framebuffer
{
	uint32_t numColorAttachments = 0;
	uint32_t width = 0;
	uint32_t height = 0;

	std::array<FramebufferAttachment, MAX_COLOR_ATTACHMENTS> colorAttachments;
	std::optional<FramebufferAttachment> depthStencilAttachment;
};

static ConcurrentObjectPool<Framebuffer> framebufferObjectPool;

FramebufferHandle CreateFramebuffer(const FramebufferCreateInfo& createInfo)
{
	EG_ASSERT(createInfo.colorAttachments.size() <= MAX_COLOR_ATTACHMENTS);

	Framebuffer* framebuffer = framebufferObjectPool.New();

	auto UpdateDimensions = [&](const FramebufferAttachment& attachment)
	{
		if (framebuffer->width == 0 && framebuffer->height == 0)
		{
			framebuffer->width = attachment.texture->width();
			framebuffer->height = attachment.texture->height();
		}
		else
		{
			EG_ASSERT(
				framebuffer->width == attachment.texture->width() &&
				framebuffer->height == attachment.texture->height());
		}
	};

	framebuffer->numColorAttachments = createInfo.colorAttachments.size();
	for (uint32_t i = 0; i < framebuffer->numColorAttachments; i++)
	{
		framebuffer->colorAttachments[i] = FramebufferAttachment::Make(createInfo.colorAttachments[i]);
		UpdateDimensions(framebuffer->colorAttachments[i]);
	}

	if (createInfo.depthStencilAttachment.texture != nullptr)
	{
		framebuffer->depthStencilAttachment = FramebufferAttachment::Make(createInfo.depthStencilAttachment);
		UpdateDimensions(*framebuffer->depthStencilAttachment);
	}

	// TODO: Handle resolve attachments

	return reinterpret_cast<FramebufferHandle>(framebuffer);
}

void DestroyFramebuffer(FramebufferHandle framebuffer)
{
	framebufferObjectPool.Delete(reinterpret_cast<Framebuffer*>(framebuffer));
}

static inline MTL::LoadAction TranslateLoadAction(AttachmentLoadOp loadOp)
{
	switch (loadOp)
	{
	case AttachmentLoadOp::Load: return MTL::LoadActionLoad;
	case AttachmentLoadOp::Clear: return MTL::LoadActionClear;
	case AttachmentLoadOp::Discard: return MTL::LoadActionDontCare;
	}
}

static void SetColorLoadStoreOp(
	MTL::RenderPassColorAttachmentDescriptor* attachmentDesc, const RenderPassColorAttachment& attachment)
{
	attachmentDesc->setLoadAction(TranslateLoadAction(attachment.loadOp));
	attachmentDesc->setStoreAction(
		attachment.finalUsage == eg::TextureUsage::Undefined ? MTL::StoreActionDontCare : MTL::StoreActionStore);

	if (attachment.loadOp == AttachmentLoadOp::Clear)
	{
		auto [clearR, clearG, clearB, clearA] = GetClearValueAs<double>(attachment.clearValue);
		attachmentDesc->setClearColor(MTL::ClearColor::Make(clearR, clearG, clearB, clearA));
	}
}

void BeginRenderPass(CommandContextHandle ctx, const RenderPassBeginInfo& beginInfo)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	MTL::RenderPassDescriptor* descriptor = MTL::RenderPassDescriptor::alloc()->init();

	int framebufferWidth, framebufferHeight;

	if (Framebuffer* framebuffer = reinterpret_cast<Framebuffer*>(beginInfo.framebuffer))
	{
		for (uint32_t i = 0; i < framebuffer->numColorAttachments; i++)
		{
			MTL::RenderPassColorAttachmentDescriptor* attachmentDesc = descriptor->colorAttachments()->object(i);
			framebuffer->colorAttachments[i].InitDescriptor(*attachmentDesc);
			SetColorLoadStoreOp(attachmentDesc, beginInfo.colorAttachments[i]);
		}

		if (framebuffer->depthStencilAttachment.has_value())
		{
			MTL::RenderPassDepthAttachmentDescriptor* attachmentDesc = descriptor->depthAttachment();
			framebuffer->depthStencilAttachment->InitDescriptor(*attachmentDesc);
			attachmentDesc->setLoadAction(TranslateLoadAction(beginInfo.depthLoadOp));
			attachmentDesc->setStoreAction(MTL::StoreActionStore);

			if (beginInfo.depthLoadOp == eg::AttachmentLoadOp::Clear)
			{
				attachmentDesc->setClearDepth(beginInfo.depthClearValue);
			}
		}

		framebufferWidth = framebuffer->width;
		framebufferHeight = framebuffer->height;
	}
	else
	{
		MTL::RenderPassColorAttachmentDescriptor* attachmentDesc = descriptor->colorAttachments()->object(0);
		attachmentDesc->setTexture(frameDrawable->texture());
		SetColorLoadStoreOp(attachmentDesc, beginInfo.colorAttachments[0]);

		framebufferWidth = frameDrawable->texture()->width();
		framebufferHeight = frameDrawable->texture()->height();
	}

	mcc.BeginRenderPass(*descriptor);
	descriptor->release();

	mcc.framebufferWidth = framebufferWidth;
	mcc.framebufferHeight = framebufferHeight;

	mtl::SetViewport(ctx, 0, 0, framebufferWidth, framebufferHeight);
	mtl::SetScissor(ctx, 0, 0, framebufferWidth, framebufferHeight);
}

void EndRenderPass(CommandContextHandle ctx)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.EndRenderPass();
}

void PushConstants(CommandContextHandle ctx, uint32_t offset, uint32_t range, const void* data)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	EG_ASSERT(offset + range <= mcc.pushConstantData.size());
	std::memcpy(mcc.pushConstantData.data() + offset, data, range);
	mcc.pushConstantsChanged = true;
}

void SetViewport(CommandContextHandle ctx, float x, float y, float w, float h)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	mcc.SetViewport(MTL::Viewport{
		.originX = static_cast<double>(x),
		.originY = static_cast<double>(y),
		.width = static_cast<double>(w),
		.height = static_cast<double>(h),
		.znear = 0.0,
		.zfar = 1.0,
	});
}

void SetScissor(CommandContextHandle ctx, int x, int y, int w, int h)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	if (mcc.boundGraphicsPipelineState != nullptr && !mcc.boundGraphicsPipelineState->enableScissorTest)
	{
		x = 0;
		y = 0;
		w = mcc.framebufferWidth;
		h = mcc.framebufferHeight;
	}

	int flippedY = std::max<int>(mcc.framebufferHeight - (y + h), 0);

	mcc.SetScissor(MTL::ScissorRect{
		.x = static_cast<NS::UInteger>(std::max<int>(x, 0)),
		.y = static_cast<NS::UInteger>(flippedY),
		.width = static_cast<NS::UInteger>(glm::clamp(w, 0, ToInt(mcc.framebufferWidth) - x)),
		.height = static_cast<NS::UInteger>(glm::clamp(h, 0, ToInt(mcc.framebufferHeight) - flippedY)),
	});
}

void SetStencilValue(CommandContextHandle, StencilValue kind, uint32_t val)
{
	EG_PANIC("Unimplemented: SetStencilValue")
}

void SetWireframe(CommandContextHandle ctx, bool wireframe)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.SetTriangleFillMode(wireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill);
}

void SetCullMode(CommandContextHandle ctx, CullMode cullMode)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.SetCullMode(TranslateCullMode(cullMode));
}

void BindIndexBuffer(CommandContextHandle ctx, IndexType type, BufferHandle buffer, uint32_t offset)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	mcc.boundIndexBuffer = UnwrapBuffer(buffer);
	mcc.boundIndexBufferOffset = offset;

	switch (type)
	{
	case IndexType::UInt16: mcc.boundIndexType = MTL::IndexType::IndexTypeUInt16; break;
	case IndexType::UInt32: mcc.boundIndexType = MTL::IndexType::IndexTypeUInt32; break;
	}
}

void BindVertexBuffer(CommandContextHandle ctx, uint32_t binding, BufferHandle buffer, uint32_t offset)
{
	MTL::RenderCommandEncoder& rce = MetalCommandContext::Unwrap(ctx).RenderCmdEncoder();
	rce.setVertexBuffer(UnwrapBuffer(buffer), offset, binding);
}

void Draw(
	CommandContextHandle ctx, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	MTL::RenderCommandEncoder& rce = mcc.RenderCmdEncoder();

	mcc.FlushDrawState();

	rce.drawPrimitives(
		mcc.boundGraphicsPipelineState->primitiveType, firstVertex, numVertices, numInstances, firstInstance);
}

void DrawIndexed(
	CommandContextHandle ctx, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance,
	uint32_t numInstances)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);
	MTL::RenderCommandEncoder& rce = mcc.RenderCmdEncoder();

	uint32_t bytesPerIndex = 2 + 2 * static_cast<uint32_t>(mcc.boundIndexType);

	mcc.FlushDrawState();

	rce.drawIndexedPrimitives(
		mcc.boundGraphicsPipelineState->primitiveType, numIndices, mcc.boundIndexType, mcc.boundIndexBuffer,
		mcc.boundIndexBufferOffset + bytesPerIndex * firstIndex, numInstances, firstVertex, firstInstance);
}

} // namespace eg::graphics_api::mtl
