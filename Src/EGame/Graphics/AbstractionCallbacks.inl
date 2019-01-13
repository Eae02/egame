#define _COMMA ,

XM_ABSCALLBACK(GetDrawableSize, std::tuple<int _COMMA int>, ())
XM_ABSCALLBACK(BeginFrame, void, ())
XM_ABSCALLBACK(EndFrame, void, ())
XM_ABSCALLBACK(Shutdown, void, ())

XM_ABSCALLBACK(CreateBuffer, BufferHandle, (BufferUsage usage, uint64_t size, const void* initialData))
XM_ABSCALLBACK(DestroyBuffer, void, (BufferHandle buffer))
XM_ABSCALLBACK(MapBuffer, void*, (BufferHandle handle, uint64_t offset, uint64_t range))
XM_ABSCALLBACK(UnmapBuffer, void, (BufferHandle handle, uint64_t modOffset, uint64_t modRange))
XM_ABSCALLBACK(UpdateBuffer, void, (BufferHandle handle, uint64_t offset, uint64_t size, const void* data))

XM_ABSCALLBACK(CreateTexture2D, TextureHandle, (const Texture2DCreateInfo& createInfo))
XM_ABSCALLBACK(CreateTexture2DArray, TextureHandle, (const Texture2DArrayCreateInfo& createInfo))
XM_ABSCALLBACK(DestroyTexture, void, (TextureHandle handle))
XM_ABSCALLBACK(SetTextureData, void, (CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, const void* data))
XM_ABSCALLBACK(SetTextureDataBuffer, void, (CommandContextHandle ctx, TextureHandle handle, const TextureRange& range, BufferHandle buffer, uint64_t offset))
XM_ABSCALLBACK(BindTexture, void, (CommandContextHandle ctx, TextureHandle texture, uint32_t binding))

XM_ABSCALLBACK(CreateSampler, SamplerHandle, (const SamplerDescription& description))
XM_ABSCALLBACK(DestroySampler, void, (SamplerHandle handle))
XM_ABSCALLBACK(BindSampler, void, (CommandContextHandle, SamplerHandle sampler, uint32_t binding))

XM_ABSCALLBACK(CreateShaderProgram, ShaderProgramHandle, (Span<const ShaderStageDesc> stages))
XM_ABSCALLBACK(DestroyShaderProgram, void, (ShaderProgramHandle handle))
XM_ABSCALLBACK(CreatePipeline, PipelineHandle, (ShaderProgramHandle program, const FixedFuncState& fixedFuncState))
XM_ABSCALLBACK(DestroyPipeline, void, (PipelineHandle handle))
XM_ABSCALLBACK(BindPipeline, void, (CommandContextHandle ctx, PipelineHandle handle))
XM_ABSCALLBACK(SetUniform, void, (CommandContextHandle ctx, ShaderProgramHandle programHandle, std::string_view name, UniformType type, uint32_t count, const void* value))

XM_ABSCALLBACK(SetViewport, void, (CommandContextHandle ctx, int x, int y, int w, int h))
XM_ABSCALLBACK(SetScissor, void, (CommandContextHandle, int x, int y, int w, int h))
XM_ABSCALLBACK(ClearFBColor, void, (CommandContextHandle ctx, int buffer, const Color& color))
XM_ABSCALLBACK(ClearFBDepth, void, (CommandContextHandle ctx, float depth))
XM_ABSCALLBACK(ClearFBStencil, void, (CommandContextHandle ctx, uint32_t value))

XM_ABSCALLBACK(BindIndexBuffer, void, (CommandContextHandle, IndexType type, BufferHandle buffer, uint32_t offset))
XM_ABSCALLBACK(BindVertexBuffer, void, (CommandContextHandle, uint32_t binding, BufferHandle buffer, uint32_t offset))

XM_ABSCALLBACK(Draw, void, (CommandContextHandle ctx, uint32_t firstVertex, uint32_t numVertices, uint32_t numInstances))
XM_ABSCALLBACK(DrawIndexed, void, (CommandContextHandle, uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t numInstances))

#undef _COMMA
