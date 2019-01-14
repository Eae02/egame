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
	typedef struct _ShaderProgram* ShaderProgramHandle;
	typedef struct _Pipeline* PipelineHandle;
	typedef struct _CommandContext* CommandContextHandle;
	
	enum class BufferUsage
	{
		None = 0,
		MapWrite = 1,
		MapRead = 2,
		Update = 4,
		CopySrc = 8,
		CopyDst = 16,
		VertexBuffer = 32,
		IndexBuffer = 64,
		UniformBuffer = 128
	};
	
	EG_BIT_FIELD(BufferUsage)
	
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
		LineLoop,
		Points
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
	
	enum class ShaderStage
	{
		Vertex = 0,
		Fragment = 1
	};
	
	struct ShaderStageDesc
	{
		ShaderStage stage;
		uint32_t codeBytes;
		const char* code;
	};
	
	enum class IndexType
	{
		UInt32,
		UInt16
	};
	
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
		OneMinusDstAlpha
	};
	
	struct BlendState
	{
		bool enabled;
		BlendFunc colorFunc;
		BlendFunc alphaFunc;
		BlendFactor srcColorFactor;
		BlendFactor srcAlphaFactor;
		BlendFactor dstColorFactor;
		BlendFactor dstAlphaFactor;
		
		BlendState()
			: enabled(false)
		{}
		
		BlendState(BlendFunc func, BlendFactor srcFactor, BlendFactor dstFactor)
			: enabled(true), colorFunc(func), alphaFunc(func), srcColorFactor(srcFactor), srcAlphaFactor(srcFactor),
			  dstColorFactor(dstFactor), dstAlphaFactor(dstFactor)
		{}
		
		BlendState(BlendFunc _colorFunc, BlendFunc _alphaFunc, BlendFactor _srcColorFactor, BlendFactor _srcAlphaFactor,
			BlendFactor _dstColorFactor, BlendFactor _dstAlphaFactor)
			: enabled(true), colorFunc(_colorFunc), alphaFunc(_alphaFunc), srcColorFactor(_srcColorFactor),
			  srcAlphaFactor(_srcAlphaFactor), dstColorFactor(_dstColorFactor), dstAlphaFactor(_dstAlphaFactor)
		{}
	};
	
	struct AttachmentState
	{
		Format format = Format::Undefined;
		uint32_t samples = 1;
		BlendState blend;
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
		uint32_t size = 0;
		uint32_t offset = 0;
		
		VertexAttribute() = default;
		VertexAttribute(uint32_t _binding, DataType _type, uint32_t _size, uint32_t _offset)
			: binding(_binding), type(_type), size(_size), offset(_offset) { }
	};
	
	struct FixedFuncState
	{
		bool enableScissorTest = false;
		bool enableDepthTest = false;
		bool enableDepthWrite = false;
		CompareOp depthCompare = CompareOp::Less;
		CullMode cullMode = CullMode::None;
		bool frontFaceCCW = false;
		Topology topology = Topology::TriangleList;
		AttachmentState attachments[8];
		VertexBinding vertexBindings[MAX_VERTEX_BINDINGS];
		VertexAttribute vertexAttributes[MAX_VERTEX_ATTRIBUTES];
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
	};
	
	struct TextureCreateInfo
	{
		uint32_t mipLevels;
		Format format;
		const SamplerDescription* defaultSamplerDescription = nullptr;
		SwizzleMode swizzleR = SwizzleMode::Identity;
		SwizzleMode swizzleG = SwizzleMode::Identity;
		SwizzleMode swizzleB = SwizzleMode::Identity;
		SwizzleMode swizzleA = SwizzleMode::Identity;
	};
	
	struct Texture2DCreateInfo : TextureCreateInfo
	{
		uint32_t width;
		uint32_t height;
	};
	
	struct Texture2DArrayCreateInfo : Texture2DCreateInfo
	{
		uint32_t arrayLayers;
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
	
	struct GraphicsCapabilities
	{
		uint32_t uniformBufferAlignment;
	};
	
	template <>
	EG_API std::string LogToString(UniformType type);
	
	bool InitializeGraphicsAPI(GraphicsAPI api, SDL_Window* window);
	
	void DestroyGraphicsAPI();
	
	namespace gal
	{
#define XM_ABSCALLBACK(name, ret, params) extern EG_API ret (*name)params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
	}
}
