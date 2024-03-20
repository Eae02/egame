#ifdef XM_WGPU_FUNC

XM_WGPU_FUNC(CreateInstance)
XM_WGPU_FUNC(InstanceRequestAdapter)
XM_WGPU_FUNC(InstanceCreateSurface)
XM_WGPU_FUNC(AdapterRequestDevice)
XM_WGPU_FUNC(AdapterEnumerateFeatures)
XM_WGPU_FUNC(AdapterGetLimits)
XM_WGPU_FUNC(AdapterGetProperties)
XM_WGPU_FUNC(DeviceCreateCommandEncoder)

XM_WGPU_FUNC(DeviceGetQueue)
XM_WGPU_FUNC(DeviceTick)
XM_WGPU_FUNC(QueueSubmit)
XM_WGPU_FUNC(QueueWriteBuffer)
XM_WGPU_FUNC(QueueOnSubmittedWorkDone)

XM_WGPU_FUNC(CommandEncoderFinish)
XM_WGPU_FUNC(CommandEncoderRelease)
XM_WGPU_FUNC(CommandBufferRelease)

XM_WGPU_FUNC(CommandEncoderBeginRenderPass)
XM_WGPU_FUNC(CommandEncoderCopyBufferToBuffer)
XM_WGPU_FUNC(CommandEncoderClearBuffer)
XM_WGPU_FUNC(CommandEncoderCopyBufferToTexture)
XM_WGPU_FUNC(CommandEncoderCopyTextureToBuffer)
XM_WGPU_FUNC(CommandEncoderCopyTextureToTexture)
XM_WGPU_FUNC(CommandEncoderWriteBuffer)

XM_WGPU_FUNC(RenderPassEncoderEnd)
XM_WGPU_FUNC(RenderPassEncoderRelease)
XM_WGPU_FUNC(RenderPassEncoderDraw)
XM_WGPU_FUNC(RenderPassEncoderDrawIndexed)
XM_WGPU_FUNC(RenderPassEncoderDrawIndexedIndirect)
XM_WGPU_FUNC(RenderPassEncoderDrawIndirect)
XM_WGPU_FUNC(RenderPassEncoderSetBindGroup)
XM_WGPU_FUNC(RenderPassEncoderSetIndexBuffer)
XM_WGPU_FUNC(RenderPassEncoderSetPipeline)
XM_WGPU_FUNC(RenderPassEncoderSetScissorRect)
XM_WGPU_FUNC(RenderPassEncoderSetStencilReference)
XM_WGPU_FUNC(RenderPassEncoderSetVertexBuffer)
XM_WGPU_FUNC(RenderPassEncoderSetViewport)

XM_WGPU_FUNC(ComputePassEncoderSetBindGroup)

XM_WGPU_FUNC(SurfaceGetPreferredFormat)
XM_WGPU_FUNC(DeviceCreateSwapChain)
XM_WGPU_FUNC(SwapChainGetCurrentTextureView)
XM_WGPU_FUNC(TextureViewRelease)
XM_WGPU_FUNC(SwapChainPresent)

XM_WGPU_FUNC(DeviceCreateBuffer)
XM_WGPU_FUNC(BufferDestroy)
XM_WGPU_FUNC(BufferGetMappedRange)
XM_WGPU_FUNC(BufferUnmap)
XM_WGPU_FUNC(BufferMapAsync)

XM_WGPU_FUNC(DeviceCreateBindGroup)
XM_WGPU_FUNC(DeviceCreateBindGroupLayout)
XM_WGPU_FUNC(BindGroupLayoutRelease)
XM_WGPU_FUNC(BindGroupRelease)

XM_WGPU_FUNC(DeviceCreateSampler)
XM_WGPU_FUNC(DeviceCreateTexture)
XM_WGPU_FUNC(TextureCreateView)
XM_WGPU_FUNC(TextureDestroy)
XM_WGPU_FUNC(TextureGetDepthOrArrayLayers)
XM_WGPU_FUNC(TextureGetDimension)
XM_WGPU_FUNC(TextureGetFormat)
XM_WGPU_FUNC(TextureGetWidth)
XM_WGPU_FUNC(TextureGetHeight)
XM_WGPU_FUNC(TextureGetMipLevelCount)
XM_WGPU_FUNC(TextureGetSampleCount)

XM_WGPU_FUNC(DeviceCreateShaderModule)
XM_WGPU_FUNC(DeviceCreateComputePipeline)
XM_WGPU_FUNC(DeviceCreateRenderPipeline)
XM_WGPU_FUNC(DeviceCreatePipelineLayout)

#endif
