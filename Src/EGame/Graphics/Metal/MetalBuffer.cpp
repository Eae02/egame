#include "MetalBuffer.hpp"
#include "MetalCommandContext.hpp"
#include "MetalPipeline.hpp"

namespace eg::graphics_api::mtl
{
BufferHandle CreateBuffer(const BufferCreateInfo& createInfo)
{
	MTL::ResourceOptions resourceOptions = {};
	if (HasFlag(createInfo.flags, BufferFlags::HostAllocate) || HasFlag(createInfo.flags, BufferFlags::MapWrite) ||
	    HasFlag(createInfo.flags, BufferFlags::MapRead) || createInfo.initialData != nullptr)
	{
		resourceOptions = MTL::ResourceStorageModeManaged;
	}
	else
	{
		resourceOptions = MTL::ResourceStorageModePrivate;
	}

	MTL::Buffer* buffer;
	if (createInfo.initialData != nullptr)
	{
		buffer = metalDevice->newBuffer(createInfo.initialData, createInfo.size, resourceOptions);
	}
	else
	{
		buffer = metalDevice->newBuffer(createInfo.size, resourceOptions);
	}

	if (createInfo.label != nullptr)
		buffer->label()->init(createInfo.label, NS::UTF8StringEncoding);

	return reinterpret_cast<BufferHandle>(buffer);
}

void DestroyBuffer(BufferHandle buffer)
{
	UnwrapBuffer(buffer)->release();
}

void BufferUsageHint(BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags) {}

void BufferBarrier(CommandContextHandle ctx, BufferHandle handle, const eg::BufferBarrier& barrier) {}

void* MapBuffer(BufferHandle handle, uint64_t offset, uint64_t range)
{
	MTL::Buffer* buffer = UnwrapBuffer(handle);
	return static_cast<char*>(buffer->contents()) + offset;
}

void FlushBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange)
{
	MTL::Buffer* buffer = UnwrapBuffer(handle);
	buffer->didModifyRange(NS::Range::Make(modOffset, modRange));
}

void InvalidateBuffer(BufferHandle handle, uint64_t modOffset, uint64_t modRange) {}

void UpdateBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, const void* data)
{
	EG_PANIC("Unimplemented: UpdateBuffer")
}

void FillBuffer(CommandContextHandle cc, BufferHandle handle, uint64_t offset, uint64_t size, uint32_t data)
{
	EG_PANIC("Unimplemented: FillBuffer")
}

void CopyBuffer(
	CommandContextHandle cc, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
{
	MTL::Buffer* msrc = UnwrapBuffer(src);
	MTL::Buffer* mdst = UnwrapBuffer(dst);

	MetalCommandContext& mcc = MetalCommandContext::Unwrap(cc);
	mcc.FlushComputeCommands();
	mcc.GetBlitCmdEncoder().copyFromBuffer(msrc, srcOffset, mdst, dstOffset, size);
}

static void BindBuffer(CommandContextHandle ctx, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset)
{
	MetalCommandContext& mcc = MetalCommandContext::Unwrap(ctx);

	MTL::Buffer* buffer = UnwrapBuffer(handle);

	if (auto location = mcc.boundGraphicsPipelineState->bindingsTableVertexShader.GetResourceMetalIndex(set, binding))
	{
		mcc.RenderCmdEncoder().setVertexBuffer(buffer, offset, *location);
	}

	if (auto location = mcc.boundGraphicsPipelineState->bindingsTableFragmentShader.GetResourceMetalIndex(set, binding))
	{
		mcc.RenderCmdEncoder().setFragmentBuffer(buffer, offset, *location);
	}
}

void BindUniformBuffer(
	CommandContextHandle cc, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset, uint64_t _range)
{
	BindBuffer(cc, handle, set, binding, offset);
}

void BindStorageBuffer(
	CommandContextHandle cc, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset, uint64_t _range)
{
	BindBuffer(cc, handle, set, binding, offset);
}
} // namespace eg::graphics_api::mtl
