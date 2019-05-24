#pragma once

#include "Graphics.hpp"
#include "Format.hpp"
#include "../Utils.hpp"
#include "../Color.hpp"
#include "../Span.hpp"
#include "../Log.hpp"

#include <tuple>

struct SDL_Window;

namespace eg
{
	constexpr uint32_t MAX_VERTEX_ATTRIBUTES = 32;
	constexpr uint32_t MAX_VERTEX_BINDINGS = 16;
	
	typedef struct _Buffer* BufferHandle;
	typedef struct _Texture* TextureHandle;
	typedef struct _Sampler* SamplerHandle;
	typedef struct _Framebuffer* FramebufferHandle;
	typedef struct _ShaderModule* ShaderModuleHandle;
	typedef struct _Pipeline* PipelineHandle;
	typedef struct _CommandContext* CommandContextHandle;
	typedef struct _DescriptorSet* DescriptorSetHandle;
	typedef struct _QueryPool* QueryPoolHandle;
	
	enum class QueryType
	{
		Timestamp,
		Occlusion
	};
	
	enum class ShaderStage
	{
		Vertex = 0,
		Fragment = 1,
		Geometry = 2,
		TessControl = 3,
		TessEvaluation = 4,
		Compute = 8
	};
	
	enum class ShaderAccessFlags
	{
		None = 0,
		Vertex = 1 << (int)ShaderStage::Vertex,
		Fragment = 1 << (int)ShaderStage::Fragment,
		Geometry = 1 << (int)ShaderStage::Geometry,
		TessControl = 1 << (int)ShaderStage::TessControl,
		TessEvaluation = 1 << (int)ShaderStage::TessEvaluation,
		Compute = 1 << (int)ShaderStage::Compute,
	};
	
	EG_BIT_FIELD(ShaderAccessFlags)
	
	enum class BufferFlags
	{
		None          = 0,
		HostAllocate  = 1,   //Allocate the buffer in host memory, if not included the buffer is allocated in device memory.
		ManualBarrier = 2,   //Barriers will be inserted manually (also disables automatic barriers).
		MapWrite      = 4,   //The buffer can be mapped for writing.
		MapRead       = 8,   //The buffer can be mapped for reading.
		Update        = 16,  //The buffer can be updated.
		CopySrc       = 32,  //Allows copy operations from the buffer to other buffers and textures.
		CopyDst       = 64,  //Allows copy operations to the buffer from other buffers.
		VertexBuffer  = 128, //The buffer can be used as a vertex buffer.
		IndexBuffer   = 256, //The buffer can be used as an index buffer.
		UniformBuffer = 512  //The buffer can be used as a uniform buffer.
	};
	
	EG_BIT_FIELD(BufferFlags)
	
	enum class BufferUsage
	{
		Undefined,
		CopySrc,
		CopyDst,
		VertexBuffer,
		IndexBuffer,
		UniformBuffer
	};
	
	struct BufferBarrier
	{
		BufferUsage oldUsage;
		BufferUsage newUsage;
		ShaderAccessFlags oldAccess = ShaderAccessFlags::None;
		ShaderAccessFlags newAccess = ShaderAccessFlags::None;
		uint64_t offset;
		uint64_t range;
	};
	
	struct BufferCreateInfo
	{
		BufferFlags flags;
		uint64_t size;
		const void* initialData = nullptr;
		const char* label = nullptr;
	};
	
	enum class CullMode
	{
		None,
		Front,
		Back
	};
	
	enum class Topology
	{
		TriangleList,
		TriangleStrip,
		TriangleFan,
		LineList,
		LineStrip,
		Points,
		Patches
	};
	
	enum class CompareOp
	{
		Never,
		Less,
		Equal,
		LessOrEqual,
		Greater,
		NotEqual,
		GreaterOrEqual,
		Always
	};
	
	enum class IndexType
	{
		UInt32,
		UInt16
	};
	
	template <typename T>
	inline IndexType GetIndexType();
	
	template <uint32_t>
	inline IndexType GetIndexType() { return IndexType::UInt32; }
	
	template <uint16_t>
	inline IndexType GetIndexType() { return IndexType::UInt16; }
	
	enum class BlendFunc
	{
		Add,
		Subtract,
		ReverseSubtract,
		Min,
		Max
	};
	
	enum class BlendFactor
	{
		Zero,
		One,
		SrcColor,
		OneMinusSrcColor,
		DstColor,
		OneMinusDstColor,
		SrcAlpha,
		OneMinusSrcAlpha,
		DstAlpha,
		OneMinusDstAlpha,
		ConstantColor,
		OneMinusConstantColor,
		ConstantAlpha,
		OneMinusConstantAlpha
	};
	
	enum class ColorWriteMask
	{
		R = 1,
		G = 2,
		B = 4,
		A = 8,
		All = 15
	};
	EG_BIT_FIELD(ColorWriteMask)
	
	struct BlendState
	{
		bool enabled;
		BlendFunc colorFunc;
		BlendFunc alphaFunc;
		BlendFactor srcColorFactor;
		BlendFactor srcAlphaFactor;
		BlendFactor dstColorFactor;
		BlendFactor dstAlphaFactor;
		ColorWriteMask colorWriteMask;
		
		BlendState()
			: enabled(false), colorFunc(BlendFunc::Add), alphaFunc(BlendFunc::Add), srcColorFactor(BlendFactor::One),
			  srcAlphaFactor(BlendFactor::One), dstColorFactor(BlendFactor::One), dstAlphaFactor(BlendFactor::One),
			  colorWriteMask(ColorWriteMask::All)
		{}
		
		BlendState(BlendFunc func, BlendFactor srcFactor, BlendFactor dstFactor,
			ColorWriteMask _colorWriteMask = ColorWriteMask::All)
			: enabled(true), colorFunc(func), alphaFunc(func), srcColorFactor(srcFactor), srcAlphaFactor(srcFactor),
			  dstColorFactor(dstFactor), dstAlphaFactor(dstFactor), colorWriteMask(_colorWriteMask)
		{}
		
		BlendState(BlendFunc _colorFunc, BlendFunc _alphaFunc, BlendFactor _srcColorFactor, BlendFactor _srcAlphaFactor,
			BlendFactor _dstColorFactor, BlendFactor _dstAlphaFactor, ColorWriteMask _colorWriteMask = ColorWriteMask::All)
			: enabled(true), colorFunc(_colorFunc), alphaFunc(_alphaFunc), srcColorFactor(_srcColorFactor),
			  srcAlphaFactor(_srcAlphaFactor), dstColorFactor(_dstColorFactor), dstAlphaFactor(_dstAlphaFactor),
			  colorWriteMask(_colorWriteMask)
		{}
	};
	
	enum class InputRate
	{
		Vertex = 0,
		Instance = 1
	};
	
	struct VertexBinding
	{
		uint32_t stride = UINT32_MAX; //If stride is UINT32_MAX, the binding is disabled
		InputRate inputRate = InputRate::Vertex;
		
		VertexBinding() = default;
		VertexBinding(uint32_t _stride, InputRate _inputRate)
			: stride(_stride), inputRate(_inputRate) { }
	};
	
	struct VertexAttribute
	{
		uint32_t binding = UINT32_MAX; //If binding is UINT32_MAX, the attribute is disabled
		DataType type = (DataType)0;
		uint32_t components = 0;
		uint32_t offset = 0;
		
		VertexAttribute() = default;
		VertexAttribute(uint32_t _binding, DataType _type, uint32_t _components, uint32_t _offset)
			: binding(_binding), type(_type), components(_components), offset(_offset) { }
	};
	
	enum class BindMode : uint8_t
	{
		Dynamic,
		DescriptorSet
	};
	
	struct SpecializationConstantEntry
	{
		uint32_t constantID;
		uint32_t offset;
		size_t size;
	};
	
	struct ShaderStageInfo
	{
		ShaderModuleHandle shaderModule;
		
		Span<const SpecializationConstantEntry> specConstants;
		size_t specConstantsDataSize = 0;
		void* specConstantsData = nullptr;
		
		ShaderStageInfo(ShaderModuleHandle _shaderModule = nullptr)
			: shaderModule(_shaderModule) { }
	};
	
	struct GraphicsPipelineCreateInfo
	{
		//Shader stages
		ShaderStageInfo vertexShader;
		ShaderStageInfo fragmentShader;
		ShaderStageInfo geometryShader;
		ShaderStageInfo tessControlShader;
		ShaderStageInfo tessEvaluationShader;
		
		//Depth & scissor
		bool enableScissorTest = false;
		bool enableDepthTest = false;
		bool enableDepthWrite = false;
		bool enableDepthClamp = false;
		CompareOp depthCompare = CompareOp::Less;
		
		//Multisampling
		bool enableAlphaToCoverage = false;
		bool enableAlphaToOne = false;
		bool enableSampleShading = false;
		float minSampleShading = 0.0f;
		
		uint32_t patchControlPoints = 0;
		uint32_t numClipDistances = 0;
		bool wireframe = false;
		CullMode cullMode = CullMode::None;
		bool frontFaceCCW = false;
		Topology topology = Topology::TriangleList;
		
		float blendConstants[4] = { };
		BindMode setBindModes[MAX_DESCRIPTOR_SETS] = { };
		
		uint32_t numColorAttachments = 1;
		BlendState blendStates[MAX_COLOR_ATTACHMENTS];
		
		VertexBinding vertexBindings[MAX_VERTEX_BINDINGS];
		VertexAttribute vertexAttributes[MAX_VERTEX_ATTRIBUTES];
		
		const char* label = nullptr;
	};
	
	enum class BindingType
	{
		UniformBuffer,
		Texture,
		StorageImage
	};
	
	struct DescriptorSetBinding
	{
		uint32_t binding;
		BindingType type;
		ShaderAccessFlags shaderAccess;
		uint32_t count;
		
		DescriptorSetBinding(uint32_t _binding, BindingType _type, ShaderAccessFlags _shaderAccess, uint32_t _count = 1)
			: binding(_binding), type(_type), shaderAccess(_shaderAccess), count(_count) { }
	};
	
	struct FramebufferFormatHint
	{
		uint32_t sampleCount = 1;
		Format depthStencilFormat = Format::Undefined;
		Format colorFormats[MAX_COLOR_ATTACHMENTS] = { };
	};
	
	struct ComputePipelineCreateInfo
	{
		ShaderStageInfo computeShader;
		BindMode setBindModes[MAX_DESCRIPTOR_SETS] = { };
		const char* label = nullptr;
	};
	
	enum class SwizzleMode
	{
		Identity,
		One,
		Zero,
		R,
		G,
		B,
		A
	};
	
	enum class WrapMode
	{
		Repeat,
		MirroredRepeat,
		ClampToEdge,
		ClampToBorder
	};
	
	enum class TextureFilter
	{
		Linear,
		Nearest
	};
	
	enum class BorderColor
	{
		F0000,
		I0000,
		F0001,
		I0001,
		F1111,
		I1111
	};
	
	struct SamplerDescription
	{
		WrapMode wrapU = WrapMode::Repeat;
		WrapMode wrapV = WrapMode::Repeat;
		WrapMode wrapW = WrapMode::Repeat;
		TextureFilter minFilter = TextureFilter::Linear;
		TextureFilter magFilter = TextureFilter::Linear;
		TextureFilter mipFilter = TextureFilter::Linear;
		float mipLodBias = 0;
		int maxAnistropy = 0;
		BorderColor borderColor = BorderColor::F0000;
		bool enableCompare = false;
		CompareOp compareOp = CompareOp::Less;
		
		bool operator==(const SamplerDescription& rhs) const;
		
		bool operator!=(const SamplerDescription& rhs) const;
	};
	
	enum class TextureUsage
	{
		Undefined,
		CopySrc,
		CopyDst,
		ShaderSample,
		FramebufferAttachment,
		ILSRead,
		ILSWrite,
		ILSReadWrite
	};
	
	enum class TextureFlags
	{
		None                  = 0,
		ManualBarrier         = 1,  //Barriers will be inserted manually (also disables automatic barriers).
		CopySrc               = 2,  //Allows copy operations from the texture to other textures and buffers.
		CopyDst               = 4,  //Allows copy operations to the texture from other textures and buffers.
		GenerateMipmaps       = 8,  //Allows automatic mipmap generation for this texture.
		ShaderSample          = 16, //The texture can be sampled in a shader.
		StorageImage          = 32, //The texture can be bound as a storage image.
		FramebufferAttachment = 64, //The texture can be used as a framebuffer attachment.
	};
	
	EG_BIT_FIELD(TextureFlags)
	
	struct TextureCreateInfo
	{
		TextureFlags flags = TextureFlags::None;
		uint32_t mipLevels = 0;
		uint32_t sampleCount = 1;
		Format format = Format::Undefined;
		const SamplerDescription* defaultSamplerDescription = nullptr;
		SwizzleMode swizzleR = SwizzleMode::Identity;
		SwizzleMode swizzleG = SwizzleMode::Identity;
		SwizzleMode swizzleB = SwizzleMode::Identity;
		SwizzleMode swizzleA = SwizzleMode::Identity;
		const char* label = nullptr;
	};
	
	struct Texture2DCreateInfo : TextureCreateInfo
	{
		uint32_t width = 0;
		uint32_t height = 0;
	};
	
	struct Texture2DArrayCreateInfo : Texture2DCreateInfo
	{
		uint32_t arrayLayers = 0;
	};
	
	struct TextureCubeCreateInfo : TextureCreateInfo
	{
		uint32_t width;
	};
	
	struct TextureCubeArrayCreateInfo : TextureCubeCreateInfo
	{
		uint32_t arrayLayers = 0;
	};
	
	enum class UniformType
	{
		Int,
		Float,
		Vec2,
		Vec3,
		Vec4,
		IVec2,
		IVec3,
		IVec4,
		Mat3,
		Mat4
	};
	
	struct TextureRange
	{
		uint32_t offsetX;
		uint32_t offsetY;
		uint32_t offsetZ;
		uint32_t sizeX;
		uint32_t sizeY;
		uint32_t sizeZ;
		uint32_t mipLevel;
	};
	
	constexpr uint32_t REMAINING_SUBRESOURCE = UINT32_MAX;
	
	struct TextureSubresource
	{
		uint32_t firstMipLevel = 0;
		uint32_t numMipLevels = REMAINING_SUBRESOURCE;
		uint32_t firstArrayLayer = 0;
		uint32_t numArrayLayers = REMAINING_SUBRESOURCE;
		
		TextureSubresource ResolveRem(uint32_t maxMipLevels, uint32_t maxArrayLayers) const;
		
		bool operator==(const TextureSubresource& rhs) const
		{
			return firstMipLevel == rhs.firstMipLevel && numMipLevels == rhs.numMipLevels &&
				firstArrayLayer == rhs.firstArrayLayer && numArrayLayers == rhs.numArrayLayers;
		}
		
		bool operator!=(const TextureSubresource& rhs) const
		{
			return !(rhs == *this);
		}
	};
	
	struct TextureSubresourceLayers
	{
		uint32_t mipLevel = 0;
		uint32_t firstArrayLayer = 0;
		uint32_t numArrayLayers = REMAINING_SUBRESOURCE;
		
		TextureSubresource AsSubresource() const
		{
			return { mipLevel, 1, firstArrayLayer, numArrayLayers };
		}
		
		TextureSubresourceLayers ResolveRem(uint32_t maxArrayLayers) const;
		
		bool operator==(const TextureSubresourceLayers& rhs) const
		{
			return mipLevel == rhs.mipLevel && firstArrayLayer == rhs.firstArrayLayer &&
			       numArrayLayers == rhs.numArrayLayers;
		}
		
		bool operator!=(const TextureSubresourceLayers& rhs) const
		{
			return !(rhs == *this);
		}
	};
	
	struct TextureBarrier
	{
		TextureUsage oldUsage;
		TextureUsage newUsage;
		ShaderAccessFlags oldAccess;
		ShaderAccessFlags newAccess;
		TextureSubresource subresource;
	};
	
	struct ResolveRegion
	{
		glm::ivec2 srcOffset;
		glm::ivec2 dstOffset;
		uint32_t width;
		uint32_t height;
		TextureSubresourceLayers srcSubresource;
		TextureSubresourceLayers dstSubresource;
	};
	
	enum class AttachmentLoadOp
	{
		Load,
		Clear,
		Discard
	};
	
	struct FramebufferAttachment
	{
		TextureHandle texture;
		TextureSubresourceLayers subresource;
		
		FramebufferAttachment(TextureHandle _texture = nullptr) : texture(_texture) { }
	};
	
	struct FramebufferCreateInfo
	{
		Span<const FramebufferAttachment> colorAttachments;
		FramebufferAttachment depthStencilAttachment;
		Span<const FramebufferAttachment> colorResolveAttachments;
		FramebufferAttachment depthStencilResolveAttachment;
	};
	
	struct RenderPassColorAttachment
	{
		AttachmentLoadOp loadOp = AttachmentLoadOp::Discard;
		ColorLin clearValue;
	};
	
	struct RenderPassBeginInfo
	{
		FramebufferHandle framebuffer;
		AttachmentLoadOp depthLoadOp = AttachmentLoadOp::Discard;
		AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::Discard;
		float depthClearValue = 1.0f;
		uint8_t stencilClearValue = 0;
		RenderPassColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
		
		explicit RenderPassBeginInfo(FramebufferHandle _framebuffer = nullptr) 
			: framebuffer(_framebuffer) { }
	};
	
	enum class DepthRange
	{
		NegOneToOne,
		ZeroToOne
	};
	
	struct GraphicsDeviceInfo
	{
		uint32_t uniformBufferAlignment;
		uint32_t maxTessellationPatchSize;
		uint32_t maxClipDistances;
		uint32_t maxComputeWorkGroupSize[3];
		uint32_t maxComputeWorkGroupCount[3];
		uint32_t maxComputeWorkGroupInvocations;
		DepthRange depthRange;
		bool geometryShader;
		bool tessellation;
		bool textureCubeMapArray;
		bool blockTextureCompression;
		float timerTicksPerNS;
		bool concurrentResourceCreation;
	};
	
	template <>
	EG_API std::string LogToString(UniformType type);
	
	struct GraphicsAPIInitArguments
	{
		SDL_Window* window;
		Format defaultDepthStencilFormat;
		bool defaultFramebufferSRGB;
		bool enableVSync;
	};
	
	namespace detail
	{
		EG_API extern GraphicsAPI graphicsAPI;
	}
	
	inline GraphicsAPI CurrentGraphicsAPI()
	{
		return detail::graphicsAPI;
	}
	
	bool InitializeGraphicsAPI(GraphicsAPI api, const GraphicsAPIInitArguments& initArguments);
	
	void DestroyGraphicsAPI();
	
	namespace gal
	{
#define XM_ABSCALLBACK(name, ret, params) extern EG_API ret (*name)params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
	}
}
