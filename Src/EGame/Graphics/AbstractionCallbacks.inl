#ifdef XM_ABSCALLBACK

XM_ABSCALLBACK(GetDrawableSize, void, (int& width, int& height))
XM_ABSCALLBACK(GetDeviceNames, std::span<std::string>, ())  
XM_ABSCALLBACK(GetDeviceInfo, void, (GraphicsDeviceInfo& deviceInfo))
XM_ABSCALLBACK(GetFormatCapabilities, FormatCapabilities, (Format format))
XM_ABSCALLBACK(EndLoading, void, ())
XM_ABSCALLBACK(IsLoadingComplete, bool, ())
XM_ABSCALLBACK(BeginFrame, void, ())
XM_ABSCALLBACK(EndFrame, void, ())
XM_ABSCALLBACK(Shutdown, void, ())
XM_ABSCALLBACK(SetEnableVSync, void, (bool enableVSync))
XM_ABSCALLBACK(DeviceWaitIdle, void, ())

XM_ABSCALLBACK(CreateCommandContext, CommandContextHandle, (Queue queue));
XM_ABSCALLBACK(DestroyCommandContext, void, (CommandContextHandle context));
XM_ABSCALLBACK(BeginRecordingCommandContext, void, (CommandContextHandle context, CommandContextBeginFlags flags));
XM_ABSCALLBACK(FinishRecordingCommandContext, void, (CommandContextHandle context));
XM_ABSCALLBACK(SubmitCommandContext, void, (CommandContextHandle context, const CommandContextSubmitArgs& args));

XM_ABSCALLBACK(CreateFence, FenceHandle, ());
XM_ABSCALLBACK(DestroyFence, void, (FenceHandle handle));
XM_ABSCALLBACK(WaitForFence, FenceStatus, (FenceHandle handle, uint64_t timeout));

XM_ABSCALLBACK(CreateBuffer, BufferHandle, (const BufferCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyBuffer, void, (BufferHandle buffer))
XM_ABSCALLBACK(BufferUsageHint, void, (BufferHandle handle, BufferUsage newUsage, ShaderAccessFlags shaderAccessFlags))
XM_ABSCALLBACK(BufferBarrier, void, (CommandContextHandle ctx, BufferHandle handle, const eg::BufferBarrier& barrier))
XM_ABSCALLBACK(MapBuffer, void*, (BufferHandle handle, uint64_t offset, std::optional<uint64_t> range))
XM_ABSCALLBACK(FlushBuffer, void, (BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange))
XM_ABSCALLBACK(InvalidateBuffer, void, (BufferHandle handle, uint64_t modOffset, std::optional<uint64_t> modRange))
XM_ABSCALLBACK(UpdateBuffer, void, (CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, const void* data))
XM_ABSCALLBACK(FillBuffer, void, (CommandContextHandle, BufferHandle handle, uint64_t offset, uint64_t size, uint8_t data))
XM_ABSCALLBACK(CopyBuffer, void, (CommandContextHandle, BufferHandle src, BufferHandle dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size))
XM_ABSCALLBACK(BindUniformBuffer, void, (CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range))
XM_ABSCALLBACK(BindStorageBuffer, void, (CommandContextHandle, BufferHandle handle, uint32_t set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range))

XM_ABSCALLBACK(CreateTexture2D, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTexture2DArray, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTextureCube, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTextureCubeArray, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTexture3D, TextureHandle, (const TextureCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyTexture, void, (TextureHandle handle))
XM_ABSCALLBACK(TextureUsageHint, void, (TextureHandle handle, TextureUsage newUsage, ShaderAccessFlags shaderAccessFlags))
XM_ABSCALLBACK(TextureBarrier, void, (CommandContextHandle ctx, TextureHandle handle, const eg::TextureBarrier& barrier))
XM_ABSCALLBACK(SetTextureData, void, (CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer, uint64_t offset))
XM_ABSCALLBACK(GetTextureData, void, (CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer, uint64_t offset))
XM_ABSCALLBACK(CopyTextureData, void, (CommandContextHandle ctx, TextureHandle src, TextureHandle dst, const TextureRange& srcRange, const TextureOffset& dstOffset))
XM_ABSCALLBACK(GenerateMipmaps, void, (CommandContextHandle ctx, TextureHandle handle))
XM_ABSCALLBACK(BindTexture, void, (CommandContextHandle ctx, TextureViewHandle textureView, SamplerHandle sampler, uint32_t set, uint32_t binding))
XM_ABSCALLBACK(BindStorageImage, void, (CommandContextHandle ctx, TextureViewHandle texture, uint32_t set, uint32_t binding))
XM_ABSCALLBACK(ResolveTexture, void, (CommandContextHandle ctx, TextureHandle src, TextureHandle dst, const ResolveRegion& region))
XM_ABSCALLBACK(GetTextureView, TextureViewHandle, (TextureHandle texture, TextureViewType viewType, const TextureSubresource& subresource, Format format))

XM_ABSCALLBACK(CreateDescriptorSetP, DescriptorSetHandle, (PipelineHandle pipeline, uint32_t set))
XM_ABSCALLBACK(CreateDescriptorSetB, DescriptorSetHandle, (std::span<const DescriptorSetBinding> bindings))
XM_ABSCALLBACK(DestroyDescriptorSet, void, (DescriptorSetHandle set))
XM_ABSCALLBACK(BindTextureDS, void, (TextureViewHandle textureView, SamplerHandle sampler, DescriptorSetHandle set, uint32_t binding))
XM_ABSCALLBACK(BindStorageImageDS, void, (TextureViewHandle textureView, DescriptorSetHandle set, uint32_t binding))
XM_ABSCALLBACK(BindUniformBufferDS, void, (BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range))
XM_ABSCALLBACK(BindStorageBufferDS, void, (BufferHandle handle, DescriptorSetHandle set, uint32_t binding, uint64_t offset, std::optional<uint64_t> range))
XM_ABSCALLBACK(BindDescriptorSet, void, (CommandContextHandle ctx, uint32_t set, DescriptorSetHandle handle, std::span<const uint32_t> dynamicOffsets))

XM_ABSCALLBACK(CreateFramebuffer, FramebufferHandle, (const FramebufferCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyFramebuffer, void, (FramebufferHandle framebuffer))

XM_ABSCALLBACK(CreateSampler, SamplerHandle, (const SamplerDescription& description))

XM_ABSCALLBACK(CreateShaderModule, ShaderModuleHandle, (ShaderStage stage, const spirv_cross::ParsedIR& parsedIR, const char* label))
XM_ABSCALLBACK(DestroyShaderModule, void, (ShaderModuleHandle handle))
XM_ABSCALLBACK(CreateGraphicsPipeline, PipelineHandle, (const GraphicsPipelineCreateInfo& createInfo))
XM_ABSCALLBACK(CreateComputePipeline, PipelineHandle, (const ComputePipelineCreateInfo& createInfo))
XM_ABSCALLBACK(GetPipelineSubgroupSize, std::optional<uint32_t>, (PipelineHandle pipeline))
XM_ABSCALLBACK(DestroyPipeline, void, (PipelineHandle handle))
XM_ABSCALLBACK(BindPipeline, void, (CommandContextHandle ctx, PipelineHandle handle))
XM_ABSCALLBACK(PushConstants, void, (CommandContextHandle ctx, uint32_t offset, uint32_t range, const void* data))

XM_ABSCALLBACK(DispatchCompute, void, (CommandContextHandle cc, uint32_t sizeX, uint32_t sizeY, uint32_t sizeZ))
XM_ABSCALLBACK(DispatchComputeIndirect, void, (CommandContextHandle cc, BufferHandle argsBuffer, uint64_t argsBufferOffset))

XM_ABSCALLBACK(SetViewport, void, (CommandContextHandle ctx, float x, float y, float w, float h))
XM_ABSCALLBACK(SetScissor, void, (CommandContextHandle, int x, int y, int w, int h))
XM_ABSCALLBACK(SetStencilValue, void, (CommandContextHandle, StencilValue kind, uint32_t val))
XM_ABSCALLBACK(SetWireframe, void, (CommandContextHandle, bool wireframe))
XM_ABSCALLBACK(SetCullMode, void, (CommandContextHandle, CullMode cullMode))
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

#endif
