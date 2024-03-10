#pragma once

#include "../Assets/Asset.hpp"
#include "Abstraction.hpp"
#include "SpirvCrossFwd.hpp"

namespace eg
{
template <typename W>
class EG_API OwningRef : public W
{
	using HandleT = std::decay_t<decltype(W::handle)>;

public:
	explicit OwningRef(HandleT _handle = nullptr) : W(_handle) {}

	~OwningRef() { W::Destroy(); }

	OwningRef(OwningRef<W>&& other) noexcept : W(other.handle) { other.handle = nullptr; }

	OwningRef<W>& operator=(OwningRef<W>&& other) noexcept
	{
		W::Destroy();
		W::handle = other.handle;
		other.handle = nullptr;
		return *this;
	}

	OwningRef(const OwningRef<W>& other) = delete;
	OwningRef<W>& operator=(const OwningRef<W>& other) = delete;
};

class EG_API PipelineRef
{
public:
	explicit PipelineRef(PipelineHandle _handle = nullptr) : handle(_handle) {}

	void Destroy()
	{
		if (handle)
		{
			gal::DestroyPipeline(handle);
			handle = nullptr;
		}
	}

	std::optional<uint32_t> TryGetSubgroupSize() const { return gal::GetPipelineSubgroupSize(handle); }

	PipelineHandle handle;
};

class EG_API Pipeline : public OwningRef<PipelineRef>
{
public:
	Pipeline() = default;
	explicit Pipeline(PipelineHandle _handle) : OwningRef(_handle) {}

	static Pipeline Create(const GraphicsPipelineCreateInfo& createInfo)
	{
		return Pipeline(gal::CreateGraphicsPipeline(createInfo));
	}

	static Pipeline Create(const ComputePipelineCreateInfo& createInfo)
	{
		return Pipeline(gal::CreateComputePipeline(createInfo));
	}
};

/**
 * Represents a shader module. The memory for shader module is reference counted,
 * so it is safe to destroy instances of this class while pipelines created from it are still alive.
 */
class EG_API ShaderModule
{
public:
	ShaderModule() = default;

	ShaderModule(ShaderStage stage, std::span<const char> code, const char* label = nullptr)
		: ShaderModule(
			  stage,
			  std::span<const uint32_t>(reinterpret_cast<const uint32_t*>(code.data()), code.size() / sizeof(uint32_t)),
			  label)
	{
	}

	ShaderModule(ShaderStage stage, std::span<const uint32_t> code, const char* label = nullptr);

	static ShaderModule CreateFromFile(const std::string& path);

	void Destroy() { m_handle.reset(); }

	/**
	 * Gets the GAL handle for this shader module.
	 */
	ShaderModuleHandle Handle() const { return m_handle.get(); }

	const spirv_cross::ParsedIR& ParsedIR() const { return *m_parsedIR; }

private:
	struct ShaderModuleDel
	{
		void operator()(ShaderModuleHandle handle) { gal::DestroyShaderModule(handle); }
	};

	std::unique_ptr<spirv_cross::ParsedIR, SpirvCrossParsedIRDeleter> m_parsedIR;

	std::unique_ptr<_ShaderModule, ShaderModuleDel> m_handle;
};

class EG_API BufferRef
{
public:
	BufferRef(BufferHandle _handle = nullptr) : handle(_handle) {}

	void* Map(uint64_t offset = 0, std::optional<uint64_t> range = std::nullopt)
	{
		return gal::MapBuffer(handle, offset, range);
	}

	void Flush(uint64_t modOffset = 0, std::optional<uint64_t> modRange = std::nullopt)
	{
		gal::FlushBuffer(handle, modOffset, modRange);
	}

	void Invalidate(uint64_t modOffset = 0, std::optional<uint64_t> modRange = std::nullopt)
	{
		gal::InvalidateBuffer(handle, modOffset, modRange);
	}

	void UsageHint(BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None)
	{
		gal::BufferUsageHint(handle, newUsage, shaderAccessFlags);
	}

	template <typename T>
	void DCUpdateData(uint64_t offset, std::span<const T> data)
	{
		DCUpdateData(offset, data.size_bytes(), data.data());
	}

	void DCUpdateData(uint64_t offset, uint64_t size, const void* data);

	void Destroy()
	{
		if (handle)
		{
			gal::DestroyBuffer(handle);
			handle = nullptr;
		}
	}

	BufferHandle handle;
};

struct UploadBuffer
{
	BufferRef buffer;
	uint64_t offset;
	uint64_t range;

	void* Map() { return buffer.Map(offset, range); }

	void Flush() { buffer.Flush(offset, range); }
};

EG_API UploadBuffer GetTemporaryUploadBuffer(uint64_t size, uint64_t alignment = 1);

template <typename T>
UploadBuffer GetTemporaryUploadBufferWith(std::span<const T> data, uint64_t alignment = 1)
{
	UploadBuffer buffer = GetTemporaryUploadBuffer(data.size_bytes(), alignment);
	std::memcpy(buffer.Map(), data.data(), data.size_bytes());
	buffer.Flush();
	return buffer;
}

EG_API void MarkUploadBuffersAvailable();
EG_API void DestroyUploadBuffers();

class EG_API Buffer : public OwningRef<BufferRef>
{
public:
	Buffer() = default;
	Buffer(BufferFlags flags, uint64_t size, const void* data)
		: OwningRef(gal::CreateBuffer(BufferCreateInfo{ flags, size, data, nullptr }))
	{
	}
	explicit Buffer(const BufferCreateInfo& createInfo) : OwningRef(gal::CreateBuffer(createInfo)) {}
};

class EG_API TextureRef
{
public:
	explicit TextureRef(TextureHandle _handle = nullptr) : handle(_handle) {}

	/**
	 * Calculates the maximum number of mip levels for a given texture resolution.
	 * @param maxDim The maximum of the texture's dimensions.
	 * @return The maximum number of mip levels.
	 */
	static uint32_t MaxMipLevels(uint32_t maxDim) { return static_cast<uint32_t>(std::log2(maxDim) + 1); }

	void UsageHint(TextureUsage usage, ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None);

	void Destroy()
	{
		if (handle != nullptr)
		{
			gal::DestroyTexture(handle);
			handle = nullptr;
		}
	}

	TextureViewHandle GetView(
		const TextureSubresource& subresource = {}, TextureViewType viewType = TextureViewType::SameAsTexture,
		Format differentFormat = Format::Undefined) const
	{
		return gal::GetTextureView(handle, viewType, subresource, differentFormat);
	}

	void DCUpdateData(const TextureRange& range, size_t dataLen, const void* data);

	TextureHandle handle;
};

class EG_API Texture : public OwningRef<TextureRef>
{
public:
	enum class LoadFormat
	{
		R_UNorm,
		RGBA_UNorm,
		RGBA_sRGB,
	};

	Texture() = default;

	/**
	 * Loads a texture from a stream containing a PNG/JPEG/TGA/BMP/GIF image.
	 * @param stream The stream to read from.
	 * @param format The format of the loaded image (not the image in the stream).
	 * @param mipLevels The number of mip levels to generate, or 0 to generate the maximum number of mip levels.
	 * @param commandContext The command context to use when uploading the image data, or null to use the direct
	 * context.
	 * @return The loaded texture, or a null texture if loading failed.
	 */
	static Texture Load(
		std::istream& stream, LoadFormat format, uint32_t mipLevels = 0,
		class CommandContext* commandContext = nullptr);

	static Texture Create2D(const TextureCreateInfo& createInfo)
	{
		Texture texture(gal::CreateTexture2D(createInfo));
		texture.m_width = createInfo.width;
		texture.m_height = createInfo.height;
		texture.m_depth = 1;
		texture.m_mipLevels = createInfo.mipLevels;
		texture.m_arrayLayers = 1;
		texture.m_format = createInfo.format;
		return texture;
	}

	static Texture Create2DArray(const TextureCreateInfo& createInfo)
	{
		Texture texture(gal::CreateTexture2DArray(createInfo));
		texture.m_width = createInfo.width;
		texture.m_height = createInfo.height;
		texture.m_depth = 1;
		texture.m_mipLevels = createInfo.mipLevels;
		texture.m_arrayLayers = createInfo.arrayLayers;
		texture.m_format = createInfo.format;
		return texture;
	}

	static Texture CreateCube(const TextureCreateInfo& createInfo)
	{
		Texture texture(gal::CreateTextureCube(createInfo));
		texture.m_width = createInfo.width;
		texture.m_height = createInfo.width;
		texture.m_depth = 1;
		texture.m_mipLevels = createInfo.mipLevels;
		texture.m_arrayLayers = 1;
		texture.m_format = createInfo.format;
		return texture;
	}

	static Texture CreateCubeArray(const TextureCreateInfo& createInfo)
	{
		Texture texture(gal::CreateTextureCubeArray(createInfo));
		texture.m_width = createInfo.width;
		texture.m_height = createInfo.width;
		texture.m_depth = 1;
		texture.m_mipLevels = createInfo.mipLevels;
		texture.m_arrayLayers = createInfo.arrayLayers;
		texture.m_format = createInfo.format;
		return texture;
	}

	static Texture Create3D(const TextureCreateInfo& createInfo)
	{
		Texture texture(gal::CreateTexture3D(createInfo));
		texture.m_width = createInfo.width;
		texture.m_height = createInfo.width;
		texture.m_depth = createInfo.depth;
		texture.m_mipLevels = createInfo.mipLevels;
		texture.m_arrayLayers = 1;
		texture.m_format = createInfo.format;
		return texture;
	}

	uint32_t Width() const { return m_width; }

	uint32_t Height() const { return m_height; }

	float WidthOverHeight() const { return static_cast<float>(m_width) / static_cast<float>(m_height); }

	uint32_t Depth() const { return m_depth; }

	uint32_t MipLevels() const { return m_mipLevels; }

	uint32_t ArrayLayers() const { return m_arrayLayers; }

	eg::Format Format() const { return m_format; }

private:
	explicit Texture(TextureHandle _handle) : OwningRef(_handle) {}

	uint32_t m_width;
	uint32_t m_height;
	uint32_t m_depth;
	uint32_t m_mipLevels;
	uint32_t m_arrayLayers;
	eg::Format m_format;
};

class EG_API FramebufferRef
{
public:
	explicit FramebufferRef(FramebufferHandle _handle = nullptr) : handle(_handle) {}

	void Destroy()
	{
		if (handle != nullptr)
		{
			gal::DestroyFramebuffer(handle);
			handle = nullptr;
		}
	}

	FramebufferHandle handle;
};

class EG_API Framebuffer : public OwningRef<FramebufferRef>
{
public:
	Framebuffer() = default;

	Framebuffer(const FramebufferCreateInfo& createInfo) { handle = gal::CreateFramebuffer(createInfo); }

	Framebuffer(std::span<const FramebufferAttachment> colorAttachments)
	{
		FramebufferCreateInfo ci;
		ci.colorAttachments = colorAttachments;
		handle = gal::CreateFramebuffer(ci);
	}

	Framebuffer(
		std::span<const FramebufferAttachment> colorAttachments, const FramebufferAttachment& depthStencilAttachment)
	{
		FramebufferCreateInfo ci;
		ci.colorAttachments = colorAttachments;
		ci.depthStencilAttachment = depthStencilAttachment;
		handle = gal::CreateFramebuffer(ci);
	}
};

class EG_API Sampler
{
public:
	Sampler() = default;
	explicit Sampler(const SamplerDescription& description);

	/**
	 * Gets the GAL handle for this sampler.
	 */
	SamplerHandle Handle() const { return m_handle; }

private:
	SamplerHandle m_handle;
};

class EG_API DescriptorSetRef
{
public:
	DescriptorSetRef(DescriptorSetHandle _handle = nullptr) : handle(_handle) {}

	void Destroy()
	{
		if (handle)
		{
			gal::DestroyDescriptorSet(handle);
			handle = nullptr;
		}
	}

	void BindTexture(
		TextureRef texture, uint32_t binding, const Sampler* sampler, const TextureSubresource& subresource = {})
	{
		BindTextureView(texture.GetView(subresource), binding, sampler);
	}

	void BindTextureView(TextureViewHandle textureView, uint32_t binding, const Sampler* sampler)
	{
		gal::BindTextureDS(textureView, sampler ? sampler->Handle() : nullptr, handle, binding);
	}

	void BindStorageImage(TextureRef texture, uint32_t binding, const TextureSubresource& subresource = {})
	{
		BindStorageImageView(texture.GetView(subresource), binding);
	}

	void BindStorageImageView(TextureViewHandle textureView, uint32_t binding)
	{
		gal::BindStorageImageDS(textureView, handle, binding);
	}

	void BindUniformBuffer(
		BufferRef buffer, uint32_t binding, uint64_t offset = 0, std::optional<uint64_t> range = std::nullopt)
	{
		gal::BindUniformBufferDS(buffer.handle, handle, binding, offset, range);
	}

	void BindStorageBuffer(
		BufferRef buffer, uint32_t binding, uint64_t offset = 0, std::optional<uint64_t> range = std::nullopt)
	{
		gal::BindStorageBufferDS(buffer.handle, handle, binding, offset, range);
	}

	DescriptorSetHandle handle;
};

class EG_API DescriptorSet : public OwningRef<DescriptorSetRef>
{
public:
	DescriptorSet() = default;

	DescriptorSet(eg::PipelineRef pipeline, uint32_t set) { handle = gal::CreateDescriptorSetP(pipeline.handle, set); }

	explicit DescriptorSet(std::span<const DescriptorSetBinding> bindings)
	{
		handle = gal::CreateDescriptorSetB(bindings);
	}
};

class EG_API QueryPoolRef
{
public:
	QueryPoolRef(QueryPoolHandle _handle = nullptr) : handle(_handle) {}

	void Destroy()
	{
		if (handle)
		{
			gal::DestroyQueryPool(handle);
			handle = nullptr;
		}
	}

	bool GetResults(uint32_t firstQuery, uint32_t numQueries, uint64_t dataSize, void* data) const
	{
		return gal::GetQueryResults(handle, firstQuery, numQueries, dataSize, data);
	}

	QueryPoolHandle handle;
};

class EG_API QueryPool : public OwningRef<QueryPoolRef>
{
public:
	QueryPool() {}
	QueryPool(QueryType type, uint32_t size) { handle = gal::CreateQueryPool(type, size); }
};

class EG_API CommandContext
{
public:
	CommandContext() : CommandContext(nullptr) {}

	static CommandContext CreateDeferred(Queue queue) { return CommandContext(gal::CreateCommandContext(queue)); }

	void SetTextureData(TextureRef texture, const TextureRange& range, BufferRef buffer, uint64_t bufferOffset)
	{
		gal::SetTextureData(Handle(), texture.handle, range, buffer.handle, bufferOffset);
	}

	void GetTextureData(TextureRef texture, const TextureRange& range, BufferRef buffer, uint64_t bufferOffset)
	{
		gal::GetTextureData(Handle(), texture.handle, range, buffer.handle, bufferOffset);
	}

	void GenerateMipmaps(TextureRef texture) { gal::GenerateMipmaps(Handle(), texture.handle); }

	void ResolveTexture(TextureRef src, TextureRef dst, const ResolveRegion& region)
	{
		gal::ResolveTexture(Handle(), src.handle, dst.handle, region);
	}

	void CopyBuffer(BufferRef src, BufferRef dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
	{
		gal::CopyBuffer(Handle(), src.handle, dst.handle, srcOffset, dstOffset, size);
	}

	void CopyTexture(TextureRef src, TextureRef dst, const TextureRange& srcRange, const TextureOffset& dstOffset)
	{
		gal::CopyTextureData(Handle(), src.handle, dst.handle, srcRange, dstOffset);
	}

	void Barrier(BufferRef buffer, const BufferBarrier& barrier)
	{
		gal::BufferBarrier(Handle(), buffer.handle, barrier);
	}

	void Barrier(TextureRef texture, const eg::TextureBarrier& barrier)
	{
		gal::TextureBarrier(Handle(), texture.handle, barrier);
	}

	void BindPipeline(PipelineRef pipeline) { gal::BindPipeline(Handle(), pipeline.handle); }

	void DispatchCompute(uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
	{
		gal::DispatchCompute(Handle(), sizeX, sizeY, sizeZ);
	}

	void DispatchComputeIndirect(BufferRef argsBuffer, uint64_t argsBufferOffset)
	{
		gal::DispatchComputeIndirect(Handle(), argsBuffer.handle, argsBufferOffset);
	}

	void UpdateBuffer(BufferRef buffer, uint64_t offset, uint64_t size, const void* data)
	{
		gal::UpdateBuffer(Handle(), buffer.handle, offset, size, data);
	}

	void FillBuffer(BufferRef buffer, uint64_t offset, uint64_t size, uint8_t data)
	{
		gal::FillBuffer(Handle(), buffer.handle, offset, size, data);
	}

	void BindVertexBuffer(uint32_t binding, BufferRef buffer, uint32_t offset)
	{
		gal::BindVertexBuffer(Handle(), binding, buffer.handle, offset);
	}

	void BindIndexBuffer(IndexType type, BufferRef buffer, uint32_t offset)
	{
		gal::BindIndexBuffer(Handle(), type, buffer.handle, offset);
	}

	void BindUniformBuffer(
		BufferRef buffer, uint32_t set, uint32_t binding, uint64_t offset = 0,
		std::optional<uint64_t> range = std::nullopt)
	{
		gal::BindUniformBuffer(Handle(), buffer.handle, set, binding, offset, range);
	}

	void BindStorageBuffer(
		BufferRef buffer, uint32_t set, uint32_t binding, uint64_t offset = 0,
		std::optional<uint64_t> range = std::nullopt)
	{
		gal::BindStorageBuffer(Handle(), buffer.handle, set, binding, offset, range);
	}

	void BindDescriptorSet(
		DescriptorSetRef descriptorSet, uint32_t setIndex, std::span<const uint32_t> dynamicOffsets = {})
	{
		gal::BindDescriptorSet(Handle(), setIndex, descriptorSet.handle, dynamicOffsets);
	}

	void Draw(uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
	{
		gal::Draw(Handle(), firstVertex, numVertices, firstInstance, numInstances);
	}

	void DrawIndexed(
		uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance, uint32_t numInstances)
	{
		gal::DrawIndexed(Handle(), firstIndex, numIndices, firstVertex, firstInstance, numInstances);
	}

	void BindTexture(
		TextureRef texture, uint32_t set, uint32_t binding, const Sampler* sampler,
		const TextureSubresource& subresource = {})
	{
		BindTextureView(texture.GetView(subresource), set, binding, sampler);
	}

	void BindTextureView(TextureViewHandle textureView, uint32_t set, uint32_t binding, const Sampler* sampler)
	{
		gal::BindTexture(Handle(), textureView, sampler ? sampler->Handle() : nullptr, set, binding);
	}

	void BindStorageImage(
		TextureRef texture, uint32_t set, uint32_t binding, const TextureSubresource& subresource = {})
	{
		BindStorageImageView(texture.GetView(subresource), set, binding);
	}

	void BindStorageImageView(TextureViewHandle textureView, uint32_t set, uint32_t binding)
	{
		gal::BindStorageImage(Handle(), textureView, set, binding);
	}

	void PushConstants(uint32_t offset, uint32_t range, const void* data)
	{
		gal::PushConstants(Handle(), offset, range, data);
	}

	template <typename T, size_t N>
	void PushConstants(uint32_t offset, T data[N])
	{
		gal::PushConstants(Handle(), offset, sizeof(T) * N, data);
	}

	template <typename T>
	void PushConstants(uint32_t offset, const T& data)
	{
		gal::PushConstants(Handle(), offset, sizeof(data), &data);
	}

	void SetViewport(float x, float y, float w, float h) { gal::SetViewport(Handle(), x, y, w, h); }

	void SetScissor(int x, int y, int w, int h) { gal::SetScissor(Handle(), x, y, w, h); }

	void SetWireframe(bool wireframe) { gal::SetWireframe(Handle(), wireframe); }
	void SetCullMode(CullMode cullMode) { gal::SetCullMode(Handle(), cullMode); }

	void SetStencilValue(StencilValue kind, uint32_t val) { gal::SetStencilValue(Handle(), kind, val); }

	void BeginRenderPass(const RenderPassBeginInfo& beginInfo) { gal::BeginRenderPass(Handle(), beginInfo); }

	void EndRenderPass() { gal::EndRenderPass(Handle()); }

	void ResetQueries(QueryPoolRef pool, uint32_t firstQuery, uint32_t numQueries)
	{
		gal::ResetQueries(Handle(), pool.handle, firstQuery, numQueries);
	}

	void BeginQuery(QueryPoolRef pool, uint32_t query) { gal::BeginQuery(Handle(), pool.handle, query); }

	void EndQuery(QueryPoolRef pool, uint32_t query) { gal::EndQuery(Handle(), pool.handle, query); }

	void WriteTimestamp(QueryPoolRef pool, uint32_t query) { gal::WriteTimestamp(Handle(), pool.handle, query); }

	void CopyQueryResults(
		QueryPoolRef pool, uint32_t firstQuery, uint32_t numQueries, BufferRef dstBuffer, uint64_t dstOffset)
	{
		gal::CopyQueryResults(Handle(), pool.handle, firstQuery, numQueries, dstBuffer.handle, dstOffset);
	}

	void DebugLabelBegin(const char* label) { gal::DebugLabelBegin(Handle(), label, nullptr); }

	void DebugLabelBegin(const char* label, const eg::ColorSRGB& color)
	{
		gal::DebugLabelBegin(Handle(), label, &color.r);
	}

	void DebugLabelEnd() { gal::DebugLabelEnd(Handle()); }

	void DebugLabelInsert(const char* label) { gal::DebugLabelInsert(Handle(), label, nullptr); }

	void DebugLabelInsert(const char* label, const eg::ColorSRGB& color)
	{
		gal::DebugLabelInsert(Handle(), label, &color.r);
	}

	/**
	 * Gets the GAL handle for this command context.
	 */
	CommandContextHandle Handle() const { return m_context.get(); }

	void BeginRecording(CommandContextBeginFlags flags) { gal::BeginRecordingCommandContext(Handle(), flags); }
	void FinishRecording() { gal::FinishRecordingCommandContext(Handle()); }
	void Submit(const CommandContextSubmitArgs& args) { gal::SubmitCommandContext(Handle(), args); }

private:
	explicit CommandContext(CommandContextHandle handle) : m_context(handle) {}

	struct CommandContextDel
	{
		void operator()(CommandContextHandle handle)
		{
			if (handle != nullptr)
				gal::DestroyCommandContext(handle);
		}
	};

	std::unique_ptr<_CommandContext, CommandContextDel> m_context;
};

extern EG_API CommandContext DC;

extern EG_API const BlendState AlphaBlend;

namespace detail
{
extern EG_API GraphicsDeviceInfo graphicsDeviceInfo;
}

inline const GraphicsDeviceInfo& GetGraphicsDeviceInfo()
{
	return detail::graphicsDeviceInfo;
}

EG_API void AssertFormatSupport(Format format, FormatCapabilities capabilities);
} // namespace eg
