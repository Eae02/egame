XM_ABSCALLBACK(GetDrawableSize, void, (int& width, int& height))
XM_ABSCALLBACK(GetDeviceInfo, void, (GraphicsDeviceInfo& deviceInfo))
XM_ABSCALLBACK(EndLoading, void, ())
XM_ABSCALLBACK(IsLoadingComplete, bool, ())
XM_ABSCALLBACK(BeginFrame, void, ())
XM_ABSCALLBACK(EndFrame, void, ())
XM_ABSCALLBACK(Shutdown, void, ())
XM_ABSCALLBACK(DeviceWaitIdle, void, ())

XM_ABSCALLBACK(CreateBuffer, BufferHandle, (const BufferCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyBuffer, void, (BufferHandle buffer))
XM_ABSCALLBACK(BufferUsageHint, void, (BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags))
XM_ABSCALLBACK(BufferBarrier, void, (CommandContextHandle ctx, BufferHandle handle, const eg::BufferBarrier& barrier))
XM_ABSCALLBACK(MapBuffer, void*, (BufferHandle handle, uint64_t offset, uint64_t range))
XM_ABSCALLBACK(FlushBuffer, void, (BufferHandle handle, uint64_t modOffset, uint64_t modRange))
XM_ABSCALLBACK(UpdateBuffer, void, (CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, const void* data))
XM_ABSCALLBACK(FillBuffer, void, (CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, uint32_t data))
XM_ABSCALLBACK(CopyBuffer, void, (CommandContextHandle, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size))
XM_ABSCALLBACK(BindUniformBuffer, void, (CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset, uint64_t range))
XM_ABSCALLBACK(BindStorageBuffer, void, (CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset, uint64_t range))

XM_ABSCALLBACK(CreateTexture2D, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTexture2DArray, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTextureCube, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTextureCubeArray, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTexture3D, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyTexture, void, (TextureHandle handle))
XM_ABSCALLBACK(TextureUsageHint, void, (TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags))
XM_ABSCALLBACK(TextureBarrier, void, (CommandContextHandle ctx, TextureHandle handle, const eg::TextureBarrier& barrier))
XM_ABSCALLBACK(SetTextureData, void, (CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer, uint64_t offset))
XM_ABSCALLBACK(CopyTextureData, void, (CommandContextHandle ctx, TextureHandle src, TextureHandle dst, const TextureRange& srcRange, const TextureOffset& dstOffset))
XM_ABSCALLBACK(GenerateMipmaps, void, (CommandContextHandle ctx, TextureHandle handle))
XM_ABSCALLBACK(BindTexture, void, (CommandContextHandle ctx, TextureHandle texture, SamplerHandle sampler, uint32_t set, uint32_t binding, const TextureSubresource& subresource))
XM_ABSCALLBACK(BindStorageImage, void, (CommandContextHandle ctx, TextureHandle texture, uint32_t set, uint32_t binding, const TextureSubresourceLayers& subresource))
XM_ABSCALLBACK(ClearColorTexture, void, (CommandContextHandle ctx, TextureHandle texture, uint32_t mipLevel, const void* color))
XM_ABSCALLBACK(ResolveTexture, void, (CommandContextHandle ctx, TextureHandle src, TextureHandle dst, const ResolveRegion& region))

XM_ABSCALLBACK(CreateDescriptorSetP, DescriptorSetHandle, (PipelineHandle pipeline, uint32_t set))
XM_ABSCALLBACK(CreateDescriptorSetB, DescriptorSetHandle, (Span<const DescriptorSetBinding> bindings))
XM_ABSCALLBACK(DestroyDescriptorSet, void, (DescriptorSetHandle set))
XM_ABSCALLBACK(BindTextureDS, void, (TextureHandle texture, SamplerHandle sampler, DescriptorSetHandle set, uint32_t binding, const TextureSubresource& subresource))
XM_ABSCALLBACK(BindStorageImageDS, void, (TextureHandle texture, DescriptorSetHandle set, uint32_t binding, const TextureSubresourceLayers& subresource))
XM_ABSCALLBACK(BindUniformBufferDS, void, (BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, uint64_t range))
XM_ABSCALLBACK(BindStorageBufferDS, void, (BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, uint64_t range))
XM_ABSCALLBACK(BindDescriptorSet, void, (CommandContextHandle ctx, uint32_t set, DescriptorSetHandle handle))

XM_ABSCALLBACK(CreateFramebuffer, FramebufferHandle, (const FramebufferCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyFramebuffer, void, (FramebufferHandle framebuffer))

XM_ABSCALLBACK(CreateSampler, SamplerHandle, (const SamplerDescription& description))
XM_ABSCALLBACK(DestroySampler, void, (SamplerHandle handle))

XM_ABSCALLBACK(CreateShaderModule, ShaderModuleHandle, (ShaderStage stage, Span<const char> code))
XM_ABSCALLBACK(DestroyShaderModule, void, (ShaderModuleHandle handle))
XM_ABSCALLBACK(CreateGraphicsPipeline, PipelineHandle, (const GraphicsPipelineCreateInfo& createInfo))
XM_ABSCALLBACK(CreateComputePipeline, PipelineHandle, (const ComputePipelineCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyPipeline, void, (PipelineHandle handle))
XM_ABSCALLBACK(PipelineFramebufferFormatHint, void, (PipelineHandle handle, const FramebufferFormatHint& hint))
XM_ABSCALLBACK(BindPipeline, void, (CommandContextHandle ctx, PipelineHandle handle))
XM_ABSCALLBACK(PushConstants, void, (CommandContextHandle ctx, uint32_t offset, uint32_t range, const void* data))

XM_ABSCALLBACK(DispatchCompute, void, (CommandContextHandle ctx, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ))

XM_ABSCALLBACK(SetViewport, void, (CommandContextHandle ctx, float x, float y, float w, float h))
XM_ABSCALLBACK(SetScissor, void, (CommandContextHandle, int x, int y, int w, int h))
XM_ABSCALLBACK(SetStencilValue, void, (CommandContextHandle, StencilValue kind, uint32_t val))
XM_ABSCALLBACK(BeginRenderPass, void, (CommandContextHandle ctx, const RenderPassBeginInfo& beginInfo))
XM_ABSCALLBACK(EndRenderPass, void, (CommandContextHandle ctx))

XM_ABSCALLBACK(BindIndexBuffer, void, (CommandContextHandle, IndexType type, BufferHandle buffer, uint32_t offset))
XM_ABSCALLBACK(BindVertexBuffer, void, (CommandContextHandle, uint32_t binding, BufferHandle buffer, uint32_t offset))

XM_ABSCALLBACK(Draw, void, (CommandContextHandle ctx, uint32_t firstVertex, uint32_t numVertices, uint32_t firstInstance, uint32_t numInstances))
XM_ABSCALLBACK(DrawIndexed, void, (CommandContextHandle, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t firstInstance, uint32_t numInstances))

XM_ABSCALLBACK(CreateQueryPool, QueryPoolHandle, (QueryType type, uint32_t queryCount))
XM_ABSCALLBACK(DestroyQueryPool, void, (QueryPoolHandle queryPool))
XM_ABSCALLBACK(GetQueryResults, bool, (QueryPoolHandle queryPool, uint32_t firstQuery, uint32_t numQueries, uint64_t dataSize, void* data))
XM_ABSCALLBACK(CopyQueryResults, void, (CommandContextHandle cctx, QueryPoolHandle queryPoolHandle,
	uint32_t firstQuery, uint32_t numQueries, BufferHandle dstBufferHandle, uint64_t dstOffset))
XM_ABSCALLBACK(WriteTimestamp, void, (CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query))
XM_ABSCALLBACK(ResetQueries, void, (CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t firstQuery, uint32_t numQueries))
XM_ABSCALLBACK(BeginQuery, void, (CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query))
XM_ABSCALLBACK(EndQuery, void, (CommandContextHandle cctx, QueryPoolHandle queryPoolHandle, uint32_t query))

XM_ABSCALLBACK(DebugLabelBegin, void, (CommandContextHandle ctx, const char* label, const float* color))
XM_ABSCALLBACK(DebugLabelEnd, void, (CommandContextHandle ctx))
XM_ABSCALLBACK(DebugLabelInsert, void, (CommandContextHandle ctx, const char* label, const float* color))
