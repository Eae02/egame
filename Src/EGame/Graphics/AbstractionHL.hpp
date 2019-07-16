#pragma once

#include "Abstraction.hpp"
#include "../Assets/Asset.hpp"

namespace eg
{
	template <typename W>
	class EG_API OwningRef : public W
	{
		using HandleT = std::decay_t<decltype(W::handle)>;
	public:
		explicit OwningRef(HandleT _handle = nullptr)
			: W(_handle) { }
		
		~OwningRef()
		{
			W::Destroy();
		}
		
		OwningRef(OwningRef<W>&& other) noexcept
			: W(other.handle)
		{
			other.handle = nullptr;
		}
		
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
		explicit PipelineRef(PipelineHandle _handle = nullptr)
			: handle(_handle) { }
		
		void Destroy()
		{
			if (handle)
			{
				gal::DestroyPipeline(handle);
				handle = nullptr;
			}
		}
		
		void FramebufferFormatHint(eg::Format colorFormat, eg::Format depthFormat = eg::Format::Undefined,
			uint32_t sampleCount = 1)
		{
			eg::FramebufferFormatHint hint;
			hint.depthStencilFormat = depthFormat;
			hint.colorFormats[0] = colorFormat;
			hint.sampleCount = sampleCount;
			gal::PipelineFramebufferFormatHint(handle, hint);
		}
		
		void FramebufferFormatHint(const eg::FramebufferFormatHint& hint)
		{
			gal::PipelineFramebufferFormatHint(handle, hint);
		}
		
		PipelineHandle handle;
	};
	
	class EG_API Pipeline : public OwningRef<PipelineRef>
	{
	public:
		Pipeline() = default;
		explicit Pipeline(PipelineHandle _handle)
			: OwningRef(_handle) { }
		
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
		
		ShaderModule(ShaderStage stage, Span<const char> code)
			: m_handle(gal::CreateShaderModule(stage, code)) { }
		
		ShaderModule(ShaderStage stage, Span<const uint32_t> code)
			: m_handle(gal::CreateShaderModule(stage, { reinterpret_cast<const char*>(code.data()), code.SizeBytes() })) { }
		
		static ShaderModule CreateFromFile(const std::string& path);
		
		/**
		 * Gets the GAL handle for this shader module.
		 */
		ShaderModuleHandle Handle() const
		{
			return m_handle.get();
		}
			
	private:
		struct ShaderModuleDel
		{
			void operator()(ShaderModuleHandle handle)
			{
				gal::DestroyShaderModule(handle);
			}
		};
		
		std::unique_ptr<_ShaderModule, ShaderModuleDel> m_handle;
	};
	
	class EG_API BufferRef
	{
	public:
		explicit BufferRef(BufferHandle _handle = nullptr)
			: handle(_handle) { }
		
		void* Map(uint64_t offset, uint64_t range)
		{
			return gal::MapBuffer(handle, offset, range);
		}
		
		void Flush(uint64_t modOffset, uint64_t modRange)
		{
			gal::FlushBuffer(handle, modOffset, modRange);
		}
		
		void UsageHint(BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None)
		{
			gal::BufferUsageHint(handle, newUsage, shaderAccessFlags);
		}
		
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
		
		void* Map()
		{
			return buffer.Map(offset, range);
		}
		
		void Flush()
		{
			buffer.Flush(offset, range);
		}
	};
	
	EG_API UploadBuffer GetTemporaryUploadBuffer(uint64_t size);
	
	EG_API void MarkUploadBuffersAvailable();
	EG_API void DestroyUploadBuffers();
	
	class EG_API Buffer : public OwningRef<BufferRef>
	{
	public:
		Buffer() = default;
		Buffer(BufferFlags flags, uint64_t size, const void* data)
			: OwningRef(gal::CreateBuffer(BufferCreateInfo { flags, size, data, nullptr })) { }
		explicit Buffer(const BufferCreateInfo& createInfo)
			: OwningRef(gal::CreateBuffer(createInfo)) { }
	};
	
	class EG_API TextureRef
	{
	public:
		explicit TextureRef(TextureHandle _handle = nullptr)
			: handle(_handle) { }
		
		/**
		 * Calculates the maximum number of mip levels for a given texture resolution.
		 * @param maxDim The maximum of the texture's dimensions.
		 * @return The maximum number of mip levels.
		 */
		static uint32_t MaxMipLevels(uint32_t maxDim)
		{
			return (uint32_t)std::log2(maxDim) + 1;
		}
		
		void UsageHint(TextureUsage usage, ShaderAccessFlags shaderAccessFlags = ShaderAccessFlags::None);
		
		void Destroy()
		{
			if (handle != nullptr)
			{
				gal::DestroyTexture(handle);
				handle = nullptr;
			}
		}
		
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
		 * @param commandContext The command context to use when uploading the image data, or null to use the direct context.
		 * @return The loaded texture, or a null texture if loading failed.
		 */
		static Texture Load(std::istream& stream, LoadFormat format, uint32_t mipLevels = 0,
			class CommandContext* commandContext = nullptr);
		
		static Texture Create2D(const Texture2DCreateInfo& createInfo)
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
		
		static Texture Create2DArray(const Texture2DArrayCreateInfo& createInfo)
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
		
		static Texture CreateCube(const TextureCubeCreateInfo& createInfo)
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
		
		static Texture CreateCubeArray(const TextureCubeArrayCreateInfo& createInfo)
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
		
		uint32_t Width() const
		{
			return m_width;
		}
		
		uint32_t Height() const
		{
			return m_height;
		}
		
		uint32_t Depth() const
		{
			return m_depth;
		}
		
		uint32_t MipLevels() const
		{
			return m_mipLevels;
		}
		
		uint32_t ArrayLayers() const
		{
			return m_arrayLayers;
		}
		
		eg::Format Format() const
		{
			return m_format;
		}
		
	private:
		explicit Texture(TextureHandle _handle)
			: OwningRef(_handle) { }
		
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
		explicit FramebufferRef(FramebufferHandle _handle = nullptr)
			: handle(_handle) { }
		
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
		
		Framebuffer(const FramebufferCreateInfo& createInfo)
		{
			handle = gal::CreateFramebuffer(createInfo);
		}
		
		Framebuffer(Span<const FramebufferAttachment> colorAttachments)
		{
			FramebufferCreateInfo ci;
			ci.colorAttachments = colorAttachments;
			handle = gal::CreateFramebuffer(ci);
		}
		
		Framebuffer(Span<const FramebufferAttachment> colorAttachments, const FramebufferAttachment& depthStencilAttachment)
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
		explicit Sampler(const SamplerDescription& description)
			: m_sampler(gal::CreateSampler(description)) { }
		
		/**
		 * Gets the GAL handle for this sampler.
		 */
		SamplerHandle Handle() const
		{
			return m_sampler.get();
		}
		
	private:
		struct SamplerDel
		{
			void operator()(SamplerHandle handle)
			{
				gal::DestroySampler(handle);
			}
		};
		
		std::unique_ptr<_Sampler, SamplerDel> m_sampler;
	};
	
	class EG_API DescriptorSetRef
	{
	public:
		DescriptorSetRef(DescriptorSetHandle _handle = nullptr)
			: handle(_handle) { }
		
		void Destroy()
		{
			if (handle)
			{
				gal::DestroyDescriptorSet(handle);
				handle = nullptr;
			}
		}
		
		void BindTexture(TextureRef texture, uint32_t binding, const Sampler* sampler = nullptr,
			const TextureSubresource& subresource = { })
		{
			gal::BindTextureDS(texture.handle, sampler ? sampler->Handle() : nullptr, handle, binding,subresource);
		}
		
		void BindStorageImage(TextureRef texture, uint32_t binding, const TextureSubresourceLayers& subresource = { })
		{
			gal::BindStorageImageDS(texture.handle, handle, binding, subresource);
		}
		
		void BindUniformBuffer(BufferRef buffer, uint32_t binding, uint64_t offset, uint64_t range)
		{
			gal::BindUniformBufferDS(buffer.handle, handle, binding, offset, range);
		}
		
		DescriptorSetHandle handle;
	};
	
	class EG_API DescriptorSet : public OwningRef<DescriptorSetRef>
	{
	public:
		DescriptorSet() = default;
		
		DescriptorSet(eg::PipelineRef pipeline, uint32_t set)
		{
			handle = gal::CreateDescriptorSetP(pipeline.handle, set);
		}
		
		explicit DescriptorSet(Span<const DescriptorSetBinding> bindings)
		{
			handle = gal::CreateDescriptorSetB(bindings);
		}
	};
	
	class EG_API QueryPoolRef
	{
	public:
		QueryPoolRef(QueryPoolHandle _handle = nullptr)
			: handle(_handle) { }
		
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
		QueryPool() { }
		QueryPool(QueryType type, uint32_t size)
		{
			handle = gal::CreateQueryPool(type, size);
		}
	};
	
	class EG_API CommandContext
	{
	public:
		CommandContext() : CommandContext(nullptr)
		{ }
		
		void SetTextureData(TextureRef texture, const TextureRange& range, BufferRef buffer, uint64_t bufferOffset)
		{
			gal::SetTextureData(Handle(), texture.handle, range, buffer.handle, bufferOffset);
		}
		
		void ClearColorTexture(TextureRef texture, uint32_t mipLevel, const Color& color)
		{
			gal::ClearColorTexture(Handle(), texture.handle, mipLevel, color);
		}
		
		void GenerateMipmaps(TextureRef texture)
		{
			gal::GenerateMipmaps(Handle(), texture.handle);
		}
		
		void ResolveTexture(TextureRef src, TextureRef dst, const ResolveRegion& region)
		{
			gal::ResolveTexture(Handle(), src.handle, dst.handle, region);
		}
		
		void CopyBuffer(BufferRef src, BufferRef dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
		{
			gal::CopyBuffer(Handle(), src.handle, dst.handle, srcOffset, dstOffset, size);
		}
		
		void Barrier(BufferRef buffer, const BufferBarrier& barrier)
		{
			gal::BufferBarrier(Handle(), buffer.handle, barrier);
		}
		
		void Barrier(TextureRef texture, const eg::TextureBarrier& barrier)
		{
			gal::TextureBarrier(Handle(), texture.handle, barrier);
		}
		
		void BindPipeline(PipelineRef pipeline)
		{
			gal::BindPipeline(Handle(), pipeline.handle);
		}
		
		void DispatchCompute(uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ)
		{
			gal::DispatchCompute(Handle(), sizeX, sizeY, sizeZ);
		}
		
		void UpdateBuffer(BufferRef buffer, uint64_t offset, uint64_t size, const void* data)
		{
			gal::UpdateBuffer(Handle(), buffer.handle, offset, size, data);
		}
		
		void BindVertexBuffer(uint32_t binding, BufferRef buffer, uint32_t offset)
		{
			gal::BindVertexBuffer(Handle(), binding, buffer.handle, offset);
		}
		
		void BindIndexBuffer(IndexType type, BufferRef buffer, uint32_t offset)
		{
			gal::BindIndexBuffer(Handle(), type, buffer.handle, offset);
		}
		
		void BindUniformBuffer(BufferRef buffer, uint32_t set, uint32_t binding, uint64_t offset, uint64_t range)
		{
			gal::BindUniformBuffer(Handle(), buffer.handle, set, binding, offset, range);
		}
		
		void BindDescriptorSet(DescriptorSetRef descriptorSet, uint32_t setIndex)
		{
			gal::BindDescriptorSet(Handle(), setIndex, descriptorSet.handle);
		}
		
		void Draw(uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances)
		{
			gal::Draw(Handle(), firstVertex, numVertices, firstInstance, numInstances);
		}
		
		void DrawIndexed(uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance, uint32_t numInstances)
		{
			gal::DrawIndexed(Handle(), firstIndex, numIndices, firstVertex, firstInstance, numInstances);
		}
		
		void BindTexture(TextureRef texture, uint32_t set, uint32_t binding, const Sampler* sampler = nullptr,
			const TextureSubresource& subresource = { })
		{
			gal::BindTexture(Handle(), texture.handle, sampler ? sampler->Handle() : nullptr, set, binding, subresource);
		}
		
		void BindStorageImage(TextureRef texture, uint32_t set, uint32_t binding,
			const TextureSubresourceLayers& subresource = { })
		{
			gal::BindStorageImage(Handle(), texture.handle, set, binding, subresource);
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
		
		void SetViewport(float x, float y, float w, float h)
		{
			gal::SetViewport(Handle(), x, y, w, h);
		}
		
		void SetScissor(int x, int y, int w, int h)
		{
			gal::SetScissor(Handle(), x, y, w, h);
		}
		
		void SetStencilValue(StencilValue kind, uint32_t val)
		{
			gal::SetStencilValue(Handle(), kind, val);
		}
		
		void BeginRenderPass(const RenderPassBeginInfo& beginInfo)
		{
			gal::BeginRenderPass(Handle(), beginInfo);
		}
		
		void EndRenderPass()
		{
			gal::EndRenderPass(Handle());
		}
		
		void ResetQueries(QueryPoolRef pool, uint32_t firstQuery, uint32_t numQueries)
		{
			gal::ResetQueries(Handle(), pool.handle, firstQuery, numQueries);
		}
		
		void BeginQuery(QueryPoolRef pool, uint32_t query)
		{
			gal::BeginQuery(Handle(), pool.handle, query);
		}
		
		void EndQuery(QueryPoolRef pool, uint32_t query)
		{
			gal::EndQuery(Handle(), pool.handle, query);
		}
		
		void WriteTimestamp(QueryPoolRef pool, uint32_t query)
		{
			gal::WriteTimestamp(Handle(), pool.handle, query);
		}
		
		void CopyQueryResults(QueryPoolRef pool, uint32_t firstQuery, uint32_t numQueries,
			BufferRef dstBuffer, uint64_t dstOffset)
		{
			gal::CopyQueryResults(Handle(), pool.handle, firstQuery, numQueries, dstBuffer.handle, dstOffset);
		}
		
		/**
		 * Gets the GAL handle for this command context.
		 */
		CommandContextHandle Handle() const
		{
			return m_context.get();
		}
		
	private:
		explicit CommandContext(CommandContextHandle handle)
			: m_context(handle) { }
		
		struct CommandContextDel
		{
			void operator()(CommandContextHandle handle)
			{
				
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
}
