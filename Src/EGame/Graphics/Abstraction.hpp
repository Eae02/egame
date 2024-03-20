#pragma once

#include "../Color.hpp"
#include "../Log.hpp"
#include "../Utils.hpp"
#include "Format.hpp"
#include "Graphics.hpp"
#include "SpirvCrossFwd.hpp"

#include <optional>
#include <span>
#include <tuple>
#include <variant>

struct SDL_Window;

namespace eg
{
constexpr uint32_t MAX_VERTEX_ATTRIBUTES = 32;
constexpr uint32_t MAX_VERTEX_BINDINGS = 16;

constexpr uint64_t BIND_BUFFER_OFFSET_DYNAMIC = UINT64_MAX;

typedef struct _Buffer* BufferHandle;
typedef struct _Texture* TextureHandle;
typedef struct _TextureView* TextureViewHandle;
typedef struct _Sampler* SamplerHandle;
typedef struct _Framebuffer* FramebufferHandle;
typedef struct _ShaderModule* ShaderModuleHandle;
typedef struct _Pipeline* PipelineHandle;
typedef struct _CommandContext* CommandContextHandle;
typedef struct _DescriptorSet* DescriptorSetHandle;
typedef struct _QueryPool* QueryPoolHandle;
typedef struct _Fence* FenceHandle;

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
	Compute = 5
};

enum class ShaderAccessFlags
{
	None = 0,
	Vertex = 1 << static_cast<int>(ShaderStage::Vertex),
	Fragment = 1 << static_cast<int>(ShaderStage::Fragment),
	Geometry = 1 << static_cast<int>(ShaderStage::Geometry),
	TessControl = 1 << static_cast<int>(ShaderStage::TessControl),
	TessEvaluation = 1 << static_cast<int>(ShaderStage::TessEvaluation),
	Compute = 1 << static_cast<int>(ShaderStage::Compute),
};

EG_BIT_FIELD(ShaderAccessFlags)

enum class BufferFlags
{
	None = 0,
	ManualBarrier = 1 << 0,     // Barriers will be inserted manually (also disables automatic barriers).
	MapWrite = 1 << 1,          // The buffer can be mapped for writing.
	MapRead = 1 << 2,           // The buffer can be mapped for reading.
	Update = 1 << 3,            // The buffer can be updated.
	CopySrc = 1 << 4,           // Allows copy operations from the buffer to other buffers and textures.
	CopyDst = 1 << 5,           // Allows copy operations to the buffer from other buffers.
	VertexBuffer = 1 << 6,      // The buffer can be used as a vertex buffer.
	IndexBuffer = 1 << 7,       // The buffer can be used as an index buffer.
	UniformBuffer = 1 << 8,     // The buffer can be used as a uniform buffer.
	StorageBuffer = 1 << 9,     // The buffer can be used as a shader storage buffer.
	IndirectCommands = 1 << 10, // The buffer can be used for arguments to indirect draw / dispatch
};

EG_BIT_FIELD(BufferFlags)

enum class BufferUsage
{
	Undefined,
	CopySrc,
	CopyDst,
	VertexBuffer,
	IndexBuffer,
	UniformBuffer,
	StorageBufferRead,
	StorageBufferWrite,
	StorageBufferReadWrite,
	HostRead,
	IndirectCommandRead,
};

struct BufferBarrier
{
	BufferUsage oldUsage;
	BufferUsage newUsage;
	ShaderAccessFlags oldAccess = ShaderAccessFlags::None;
	ShaderAccessFlags newAccess = ShaderAccessFlags::None;
	uint64_t offset = 0;
	std::optional<uint64_t> range;
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
inline IndexType GetIndexType()
{
	return IndexType::UInt32;
}

template <uint16_t>
inline IndexType GetIndexType()
{
	return IndexType::UInt16;
}

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
	{
	}

	BlendState(
		BlendFunc func, BlendFactor srcFactor, BlendFactor dstFactor,
		ColorWriteMask _colorWriteMask = ColorWriteMask::All)
		: enabled(true), colorFunc(func), alphaFunc(func), srcColorFactor(srcFactor), srcAlphaFactor(srcFactor),
		  dstColorFactor(dstFactor), dstAlphaFactor(dstFactor), colorWriteMask(_colorWriteMask)
	{
	}

	BlendState(
		BlendFunc _colorFunc, BlendFunc _alphaFunc, BlendFactor _srcColorFactor, BlendFactor _srcAlphaFactor,
		BlendFactor _dstColorFactor, BlendFactor _dstAlphaFactor, ColorWriteMask _colorWriteMask = ColorWriteMask::All)
		: enabled(true), colorFunc(_colorFunc), alphaFunc(_alphaFunc), srcColorFactor(_srcColorFactor),
		  srcAlphaFactor(_srcAlphaFactor), dstColorFactor(_dstColorFactor), dstAlphaFactor(_dstAlphaFactor),
		  colorWriteMask(_colorWriteMask)
	{
	}
};

enum class InputRate
{
	Vertex = 0,
	Instance = 1
};

struct VertexBinding
{
	uint32_t stride = UINT32_MAX; // If stride is UINT32_MAX, the binding is disabled
	InputRate inputRate = InputRate::Vertex;

	bool IsEnabled() const { return stride != UINT32_MAX; }

	VertexBinding() = default;
	VertexBinding(uint32_t _stride, InputRate _inputRate) : stride(_stride), inputRate(_inputRate) {}
};

struct VertexAttribute
{
	uint32_t binding = UINT32_MAX; // If binding is UINT32_MAX, the attribute is disabled
	Format format = Format::Undefined;
	uint32_t offset = 0;

	bool IsEnabled() const { return binding != UINT32_MAX; }

	VertexAttribute() = default;
	VertexAttribute(uint32_t _binding, Format _format, uint32_t _offset)
		: binding(_binding), format(_format), offset(_offset)
	{
	}
	VertexAttribute(uint32_t _binding, DataType _type, uint32_t _components, uint32_t _offset)
		: binding(_binding), format(FormatFromDataTypeAndComponentCount(_type, _components)), offset(_offset)
	{
	}
};

enum class BindMode : uint8_t
{
	Dynamic,
	DescriptorSet
};

struct SpecializationConstantEntry
{
	uint32_t constantID;
	std::variant<uint32_t, int32_t, float> value;
};

EG_API std::optional<std::variant<uint32_t, int32_t, float>> GetSpecConstantValueByID(
	std::span<const SpecializationConstantEntry> specConstants, uint32_t id);

struct ShaderStageInfo
{
	ShaderModuleHandle shaderModule = nullptr;
	std::span<const SpecializationConstantEntry> specConstants;
};

enum class StencilOp
{
	Keep,
	Zero,
	Replace,
	IncrementAndClamp,
	DecrementAndClamp,
	Invert,
	IncrementAndWrap,
	DecrementAndWrap
};

struct StencilState
{
	StencilOp failOp;
	StencilOp passOp;
	StencilOp depthFailOp;
	CompareOp compareOp;
	uint32_t compareMask;
	uint32_t writeMask;
	uint32_t reference;
};

static constexpr int STENCIL_VALUE_MASK_VALUE = 0b0011;
static constexpr int STENCIL_VALUE_COMPARE_MASK = 0b0000;
static constexpr int STENCIL_VALUE_WRITE_MASK = 0b0001;
static constexpr int STENCIL_VALUE_REFERENCE = 0b0010;
static constexpr int STENCIL_VALUE_MASK_BACK = 0b1000;
static constexpr int STENCIL_VALUE_MASK_FRONT = 0b0100;

enum class StencilValue
{
	FrontCompareMask = 0b0100,
	FrontWriteMask = 0b0101,
	FrontReference = 0b0110,
	BackCompareMask = 0b1000,
	BackWriteMask = 0b1001,
	BackReference = 0b1010,
	CompareMask = 0b1100,
	WriteMask = 0b1101,
	Reference = 0b1110
};

enum class BindingType
{
	UniformBuffer,
	StorageBuffer,
	UniformBufferDynamicOffset,
	StorageBufferDynamicOffset,
	Texture,
	StorageImage
};

inline bool BindingTypeHasDynamicOffset(BindingType type)
{
	return type == BindingType::StorageBufferDynamicOffset || type == BindingType::UniformBufferDynamicOffset;
}

std::string_view BindingTypeToString(BindingType bindingType);

enum class ReadWriteMode
{
	ReadWrite,
	ReadOnly,
	WriteOnly,
};

struct DescriptorSetBinding
{
	uint32_t binding = 0;
	BindingType type = BindingType::UniformBuffer;
	ShaderAccessFlags shaderAccess = ShaderAccessFlags::None;
	ReadWriteMode rwMode = ReadWriteMode::ReadWrite;

	bool operator==(const DescriptorSetBinding&) const = default;
	bool operator!=(const DescriptorSetBinding&) const = default;

	size_t Hash() const;

	struct BindingCmp
	{
		bool operator()(const DescriptorSetBinding& a, const DescriptorSetBinding& b) const
		{
			return a.binding < b.binding;
		}
	};

	static uint32_t MaxBindingPlusOne(std::span<const DescriptorSetBinding> bindings)
	{
		uint32_t maxBindingPlusOne = 0;
		for (const DescriptorSetBinding& binding : bindings)
			maxBindingPlusOne = std::max(maxBindingPlusOne, binding.binding + 1);
		return maxBindingPlusOne;
	}
};

struct GraphicsPipelineCreateInfo
{
	// Shader stages
	ShaderStageInfo vertexShader;
	ShaderStageInfo fragmentShader;
	ShaderStageInfo geometryShader;
	ShaderStageInfo tessControlShader;
	ShaderStageInfo tessEvaluationShader;

	// Depth & scissor
	bool enableScissorTest = false;
	bool enableDepthTest = false;
	bool enableDepthWrite = false;
	bool enableDepthClamp = false;
	CompareOp depthCompare = CompareOp::Less;

	// Stencil
	bool enableStencilTest = false;
	StencilState frontStencilState;
	StencilState backStencilState;
	bool dynamicStencilCompareMask = false;
	bool dynamicStencilWriteMask = false;
	bool dynamicStencilReference = false;

	// Multisampling
	bool enableAlphaToCoverage = false;
	bool enableAlphaToOne = false;
	bool enableSampleShading = false;
	float minSampleShading = 0.0f;

	uint32_t patchControlPoints = 0;
	uint32_t numClipDistances = 0;
	float lineWidth = 1;
	bool frontFaceCCW = false;
	Topology topology = Topology::TriangleList;

	// nullopt means that cull mode is set dynamically by calling SetCullMode
	std::optional<CullMode> cullMode = CullMode::None;

	// Setting this this true means that wireframe rasterization can be enabled by calling SetWireframe
	bool enableWireframeRasterization = false;

	std::array<float, 4> blendConstants = {};
	BindMode setBindModes[MAX_DESCRIPTOR_SETS] = {};
	std::span<const eg::DescriptorSetBinding> descriptorSetBindings[MAX_DESCRIPTOR_SETS];

	uint32_t numColorAttachments = 1;
	Format colorAttachmentFormats[MAX_COLOR_ATTACHMENTS] = {};
	BlendState blendStates[MAX_COLOR_ATTACHMENTS];

	uint32_t sampleCount = 1;

	Format depthAttachmentFormat = Format::Undefined;

	VertexBinding vertexBindings[MAX_VERTEX_BINDINGS];
	VertexAttribute vertexAttributes[MAX_VERTEX_ATTRIBUTES];

	const char* label = nullptr;
};

struct ComputePipelineCreateInfo
{
	ShaderStageInfo computeShader;
	BindMode setBindModes[MAX_DESCRIPTOR_SETS] = {};
	bool requireFullSubgroups = false;
	std::optional<uint32_t> requiredSubgroupSize;
	const char* label = nullptr;
};

enum class Queue
{
	Main,
	ComputeOnly,
};

enum class CommandContextBeginFlags
{
	OneTimeSubmit = 0x1,
	SimultaneousUse = 0x2,
};
EG_BIT_FIELD(CommandContextBeginFlags);

struct CommandContextSubmitArgs
{
	FenceHandle fence;
};

enum class FenceStatus
{
	Signaled,
	Timeout,
	Error,
};

enum class WrapMode
{
	Repeat,
	MirroredRepeat,
	ClampToEdge,
};

enum class TextureFilter
{
	Linear,
	Nearest
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
	float minLod = -1000.0f;
	float maxLod = 1000.0f;
	int maxAnistropy = 0;
	bool enableCompare = false;
	CompareOp compareOp = CompareOp::Less;

	bool operator==(const SamplerDescription&) const = default;
	bool operator!=(const SamplerDescription&) const = default;

	size_t Hash() const;
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
	None = 0,

	// Barriers will be inserted manually (also disables automatic barriers).
	ManualBarrier = 1,

	// Allows copy operations from the texture to other textures and buffers.
	CopySrc = 2,

	// Allows copy operations to the texture from other textures and buffers.
	CopyDst = 4,

	// Allows automatic mipmap generation for this texture.
	GenerateMipmaps = 8,

	// The texture can be sampled in a shader.
	ShaderSample = 16,

	// The texture can be bound as a storage image.
	StorageImage = 32,

	// The texture can be used as a framebuffer attachment.
	FramebufferAttachment = 64,
};

enum class TextureViewType
{
	SameAsTexture,
	Flat2D,
	Flat3D,
	Cube,
	Array2D,
	ArrayCube,
};

EG_BIT_FIELD(TextureFlags)

struct TextureCreateInfo
{
	TextureFlags flags = TextureFlags::None;
	uint32_t mipLevels = 0;
	uint32_t sampleCount = 1;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t depth = 0;
	uint32_t arrayLayers = 1;
	Format format = Format::Undefined;
	const char* label = nullptr;
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

struct TextureOffset
{
	uint32_t offsetX;
	uint32_t offsetY;
	uint32_t offsetZ;
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

	bool operator!=(const TextureSubresource& rhs) const { return !(rhs == *this); }

	size_t Hash() const;
};

struct TextureSubresourceLayers
{
	uint32_t mipLevel = 0;
	uint32_t firstArrayLayer = 0;
	uint32_t numArrayLayers = REMAINING_SUBRESOURCE;

	TextureSubresource AsSubresource() const { return { mipLevel, 1, firstArrayLayer, numArrayLayers }; }

	TextureSubresourceLayers ResolveRem(uint32_t maxArrayLayers) const;

	bool operator==(const TextureSubresourceLayers& rhs) const
	{
		return mipLevel == rhs.mipLevel && firstArrayLayer == rhs.firstArrayLayer &&
		       numArrayLayers == rhs.numArrayLayers;
	}

	bool operator!=(const TextureSubresourceLayers& rhs) const { return !(rhs == *this); }

	size_t Hash() const;
};

struct TextureViewKey
{
	TextureViewType type;
	Format format;
	TextureSubresource subresource;

	bool operator==(const TextureViewKey&) const = default;
	bool operator!=(const TextureViewKey&) const = default;

	size_t Hash() const;
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

	FramebufferAttachment(TextureHandle _texture = nullptr) : texture(_texture) {}
};

struct FramebufferCreateInfo
{
	std::span<const FramebufferAttachment> colorAttachments;
	FramebufferAttachment depthStencilAttachment;
	std::span<const FramebufferAttachment> colorResolveAttachments;
	FramebufferAttachment depthStencilResolveAttachment;
	const char* label = nullptr;
};

struct RenderPassColorAttachment
{
	AttachmentLoadOp loadOp = AttachmentLoadOp::Discard;

	// Set to undefined to discard the attachment data
	TextureUsage finalUsage = TextureUsage::FramebufferAttachment;

	std::variant<ColorLin, glm::ivec4, glm::uvec4> clearValue;
};

struct RenderPassBeginInfo
{
	FramebufferHandle framebuffer = nullptr;
	AttachmentLoadOp depthLoadOp = AttachmentLoadOp::Discard;
	AttachmentLoadOp stencilLoadOp = AttachmentLoadOp::Discard;
	bool sampledDepthStencil = false;
	float depthClearValue = 1.0f;
	uint8_t stencilClearValue = 0;
	RenderPassColorAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
};

enum class DepthRange
{
	NegOneToOne,
	ZeroToOne
};

enum class DeviceFeatureFlags
{
	ComputeShaderAndSSBO = 1 << 0,
	GeometryShader = 1 << 1,
	TessellationShader = 1 << 2,
	PartialTextureViews = 1 << 3,
	TextureCubeMapArray = 1 << 4,
	TextureCompressionBC = 1 << 5,
	TextureCompressionASTC = 1 << 6,
	ConcurrentResourceCreation = 1 << 7,
	DynamicResourceBind = 1 << 8,
	DeferredContext = 1 << 9,
};

EG_BIT_FIELD(DeviceFeatureFlags)

enum class SubgroupFeatureFlags
{
	// Subgroup flags must have the same values as in VkSubgroupFeatureFlags
	Basic = 1 << 0,
	Vote = 1 << 1,
	Arithmetic = 1 << 2,
	Ballot = 1 << 3,
	Shuffle = 1 << 4,
	ShuffleRelative = 1 << 5,
	Clustered = 1 << 6,
	Quad = 1 << 7,
};

EG_BIT_FIELD(SubgroupFeatureFlags)

struct SubgroupFeatures
{
	uint32_t minSubgroupSize;
	uint32_t maxSubgroupSize;
	uint32_t maxWorkgroupSubgroups;
	bool supportsRequireFullSubgroups;
	bool supportsRequiredSubgroupSize;
	bool subgroupUniformControlFlow;
	bool supportsGetPipelineSubgroupSize;
	SubgroupFeatureFlags featureFlags;
};

struct GraphicsDeviceInfo
{
	uint32_t uniformBufferOffsetAlignment;
	uint32_t storageBufferOffsetAlignment;
	uint32_t maxTessellationPatchSize;
	uint32_t maxClipDistances;
	uint32_t maxComputeWorkGroupSize[3];
	uint32_t maxComputeWorkGroupCount[3];
	uint32_t maxComputeWorkGroupInvocations;
	std::optional<SubgroupFeatures> subgroupFeatures;
	DepthRange depthRange;
	DeviceFeatureFlags features;
	float timerTicksPerNS;

	std::string_view deviceName;
	std::string_view apiName;
};

template <>
EG_API std::string LogToString(UniformType type);

struct GraphicsAPIInitArguments
{
	SDL_Window* window;
	Format defaultDepthStencilFormat;
	bool defaultFramebufferSRGB;
	bool forceDepthZeroToOne;
	bool preferIntegrated;
	bool preferGLESPath;
	std::string_view preferredDeviceName;
};

namespace detail
{
EG_API extern GraphicsAPI graphicsAPI;
} // namespace detail

inline GraphicsAPI CurrentGraphicsAPI()
{
	return detail::graphicsAPI;
}

bool InitializeGraphicsAPI(GraphicsAPI api, const GraphicsAPIInitArguments& initArguments);

void DestroyGraphicsAPI();

struct GraphicsMemoryStat
{
	uint64_t allocatedBytes;
	uint64_t allocatedBytesGPU;
	uint32_t numBlocks;
	uint32_t unusedRanges;
};

namespace gal
{
#define XM_ABSCALLBACK(name, ret, params) EG_API extern ret(*name) params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK

EG_API extern GraphicsMemoryStat (*GetMemoryStat)();
} // namespace gal
} // namespace eg
