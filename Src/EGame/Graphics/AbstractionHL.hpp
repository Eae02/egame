#pragma once

#include "Abstraction.hpp"
#include "../Assets/ShaderModule.hpp"
#include "../Assets/Asset.hpp"

namespace eg
{
	template <typename W>
	class OwningRef : public W
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
	};
	
	class EG_API Pipeline
	{
	public:
		inline explicit Pipeline(PipelineHandle handle = nullptr)
			: m_pipeline(handle) { }
		
		/**
		 * Gets the GAL handle for this pipeline.
		 */
		PipelineHandle Handle() const
		{
			return m_pipeline.get();
		}
		
	private:
		struct PipelineDel
		{
			void operator()(PipelineHandle handle)
			{
				gal::DestroyPipeline(handle);
			}
		};
		
		std::unique_ptr<_Pipeline, PipelineDel> m_pipeline;
	};
	
	/**
	 * Represents a shader program. The memory for shader programs is reference counted,
	 * so it is safe to destroy instances of this class while pipelines created from it are still alive.
	 */
	class EG_API ShaderProgram
	{
	public:
		ShaderProgram() = default;
		
		void AddStageFromAsset(std::string_view name)
		{
			ShaderModule& module = GetAsset<ShaderModule>(name);
			AddStageBorrowedCode(module.stage, module.code);
		}
		
		/**
		 * Adds a stage to the shader program.
		 * @param stage The stage to add
		 * @param code SPIR-V code for the stage to be added
		 */
		void AddStage(ShaderStage stage, std::vector<char> code)
		{
			AddStageBorrowedCode(stage, code);
			m_code.push_back(std::move(code));
		}
		
		/**
		 * Adds a stage to the shader program, the memory pointed
		 * to by code must stay alive until CreatePipeline is called.
		 * @param stage The stage to add
		 * @param code SPIR-V code for the stage to be added
		 */
		void AddStageBorrowedCode(ShaderStage stage, Span<const char> code)
		{
			m_stages.push_back({ stage, (uint32_t)code.size(), code.data() });
			m_program = nullptr;
		}
		
		/**
		 * Adds a stage to the shader program by reading from a spir-v file.
		 * The shader stage is deduced from the file extension. TODO: How?
		 * @param path The path to the spir-v file.
		 */
		void AddStageFromFile(const std::string& path);
		
		/**
		 * Creates a pipeline using this shader program.
		 * @param fixedFuncState 
		 * @return The created pipeline.
		 */
		Pipeline CreatePipeline(const FixedFuncState& fixedFuncState)
		{
			if (m_program == nullptr)
				m_program.reset(gal::CreateShaderProgram(m_stages));
			return Pipeline(gal::CreatePipeline(Handle(), fixedFuncState));
		}
		
		/**
		 * Gets the GAL handle for this shader program.
		 */
		ShaderProgramHandle Handle() const
		{
			return m_program.get();
		}
		
	private:
		struct ShaderProgramDel
		{
			void operator()(ShaderProgramHandle handle)
			{
				gal::DestroyShaderProgram(handle);
			}
		};
		
		std::vector<std::vector<char>> m_code;
		std::vector<ShaderStageDesc> m_stages;
		std::unique_ptr<_ShaderProgram, ShaderProgramDel> m_program;
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
		
		void Unmap(uint64_t modOffset, uint64_t modRange)
		{
			gal::UnmapBuffer(handle, modOffset, modRange);
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
	
	EG_API BufferRef GetTemporaryUploadBuffer(uint64_t size);
	
	EG_API void MarkUploadBuffersAvailable();
	EG_API void DestroyUploadBuffers();
	
	class EG_API Buffer : public OwningRef<BufferRef>
	{
	public:
		Buffer() = default;
		Buffer(BufferUsage usage, MemoryType memoryType, uint64_t size, const void* data)
			: OwningRef(gal::CreateBuffer(usage, memoryType, size, data)) { }
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
		explicit Texture(TextureHandle _handle = nullptr)
			: OwningRef(_handle) { }
		
		enum class LoadFormat
		{
			R_UNorm,
			RGBA_UNorm,
			RGBA_sRGB,
		};
		
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
			return Texture(gal::CreateTexture2D(createInfo));
		}
		
		static Texture Create2DArray(const Texture2DArrayCreateInfo& createInfo)
		{
			return Texture(gal::CreateTexture2DArray(createInfo));
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
	
	class EG_API CommandContext
	{
	public:
		CommandContext() : CommandContext(nullptr) { }
		
		void SetTextureData(TextureRef texture, const TextureRange& range, const void* data)
		{
			gal::SetTextureData(Handle(), texture.handle, range, data);
		}
		
		void SetTextureData(TextureRef texture, const TextureRange& range, BufferRef buffer, uint64_t bufferOffset)
		{
			gal::SetTextureDataBuffer(Handle(), texture.handle, range, buffer.handle, bufferOffset);
		}
		
		void GenerateMipmaps(TextureRef texture)
		{
			gal::GenerateMipmaps(Handle(), texture.handle);
		}
		
		void CopyBuffer(BufferRef src, BufferRef dst, uint64_t srcOffset, uint64_t dstOffset, uint64_t size)
		{
			gal::CopyBuffer(Handle(), src.handle, dst.handle, srcOffset, dstOffset, size);
		}
		
		void BindPipeline(const Pipeline& pipeline)
		{
			gal::BindPipeline(Handle(), pipeline.Handle());
		}
		
		void BindVertexBuffer(uint32_t binding, BufferRef buffer, uint32_t offset)
		{
			gal::BindVertexBuffer(Handle(), binding, buffer.handle, offset);
		}
		
		void BindIndexBuffer(IndexType type, BufferRef buffer, uint32_t offset)
		{
			gal::BindIndexBuffer(Handle(), type, buffer.handle, offset);
		}
		
		void BindUniformBuffer(BufferRef buffer, uint32_t binding, uint64_t offset, uint64_t range)
		{
			gal::BindUniformBuffer(Handle(), buffer.handle, binding, offset, range);
		}
		
		void Draw(uint32_t firstVertex, uint32_t numVertices, uint32_t numInstances)
		{
			gal::Draw(Handle(), firstVertex, numVertices, numInstances);
		}
		
		void DrawIndexed(uint32_t firstIndex, uint32_t numIndices, uint32_t firstVertex, uint32_t numInstances)
		{
			gal::DrawIndexed(Handle(), firstIndex, numIndices, firstVertex, numInstances);
		}
		
		void BindTexture(TextureRef texture, uint32_t binding)
		{
			gal::BindTexture(Handle(), texture.handle, binding);
		}
		
		void BindSampler(const Sampler& sampler, uint32_t binding)
		{
			gal::BindSampler(Handle(), sampler.Handle(), binding);
		}
		
		void SetUniform(std::string_view name, UniformType type, const void* value)
		{
			gal::SetUniform(Handle(), nullptr, name, type, 1, value);
		}
		
		void SetUniform(std::string_view name, UniformType type, uint32_t count, const void* value)
		{
			gal::SetUniform(Handle(), nullptr, name, type, count, value);
		}
		
		void SetUniform(const ShaderProgram& program, std::string_view name, UniformType type, const void* value)
		{
			gal::SetUniform(Handle(), program.Handle(), name, type, 1, value);
		}
		
		void SetUniform(const ShaderProgram& program, std::string_view name, UniformType type, uint32_t count, const void* value)
		{
			gal::SetUniform(Handle(), program.Handle(), name, type, count, value);
		}
		
		void SetViewport(int x, int y, int w, int h)
		{
			gal::SetViewport(Handle(), x, y, w, h);
		}
		
		void SetScissor(int x, int y, int w, int h)
		{
			gal::SetScissor(Handle(), x, y, w, h);
		}
		
		/**
		 * Clears a color attachment from the current framebuffer.
		 * @param buffer The attachment to clear.
		 * @param color The color to set all pixels to.
		 */
		void ClearColor(int buffer, const Color& color)
		{
			gal::ClearFBColor(Handle(), buffer, color);
		}
		
		/**
		 * Clears the current framebuffer's depth attachment.
		 * @param depth The depth value to set all pixels to.
		 */
		void ClearDepth(float depth)
		{
			gal::ClearFBDepth(Handle(), depth);
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
		extern EG_API GraphicsCapabilities graphicsCapabilities;
	}
	
	inline const GraphicsCapabilities& GraphicsCaps()
	{
		return detail::graphicsCapabilities;
	}
}
