#include "Abstraction.hpp"
#include "../Assert.hpp"
#include "../Hash.hpp"
#include "../Log.hpp"
#include "../MainThreadInvoke.hpp"

#include "OpenGL/OpenGL.hpp"

#ifdef EG_ENABLE_WEBGPU
#include "WebGPU/WGPUMain.hpp"
#include "WebGPU/WGPUPlatform.hpp"
#endif

#ifdef EG_ENABLE_VULKAN
#include "Vulkan/VulkanMain.hpp"
#endif

#ifdef __APPLE__
#include "Metal/MetalMain.hpp"
#endif

namespace eg
{
size_t SamplerDescription::Hash() const
{
	size_t h = 0;
	h |= static_cast<size_t>(wrapU) << 0;
	h |= static_cast<size_t>(wrapV) << 2;
	h |= static_cast<size_t>(wrapW) << 4;
	h |= static_cast<size_t>(minFilter) << 6;
	h |= static_cast<size_t>(magFilter) << 7;
	h |= static_cast<size_t>(mipFilter) << 8;
	h |= static_cast<size_t>(enableCompare) << 9;
	h |= static_cast<size_t>(compareOp) << 10;

	HashAppend(h, maxAnistropy);
	HashAppend(h, mipLodBias);
	HashAppend(h, minLod);
	HashAppend(h, maxLod);

	return h;
}

size_t BindingTypeTexture::Hash() const
{
	return static_cast<size_t>(viewType) | (static_cast<size_t>(sampleMode) << 10) |
	       (static_cast<size_t>(multisample) << 20);
}

size_t BindingTypeStorageImage::Hash() const
{
	return static_cast<size_t>(viewType) | (static_cast<size_t>(rwMode) << 10) | (static_cast<size_t>(format) << 20);
}

size_t DescriptorSetBinding::Hash() const
{
	size_t h = 0;
	HashAppend(h, shaderAccess);
	HashAppend(h, binding);
	std::visit(
		[&]<typename T>(const T& t)
		{
			if constexpr (std::is_same_v<BindingTypeSampler, T>)
				HashAppend(h, t);
			else
				HashAppend(h, t.Hash());
		},
		type);
	return h;
}

BindingType DescriptorSetBinding::GetBindingType() const
{
	if (std::holds_alternative<BindingTypeTexture>(type))
		return BindingType::Texture;
	if (std::holds_alternative<BindingTypeStorageImage>(type))
		return BindingType::StorageImage;
	if (std::holds_alternative<BindingTypeStorageBuffer>(type))
		return BindingType::StorageBuffer;
	if (std::holds_alternative<BindingTypeUniformBuffer>(type))
		return BindingType::UniformBuffer;
	if (std::holds_alternative<BindingTypeSampler>(type))
		return BindingType::Sampler;
	EG_UNREACHABLE
}

size_t TextureViewKey::Hash() const
{
	size_t h = subresource.Hash();
	HashAppend(h, static_cast<uint32_t>(type));
	HashAppend(h, static_cast<uint32_t>(format));
	return h;
}

namespace gal
{
GraphicsMemoryStat (*GetMemoryStat)();

#define XM_ABSCALLBACK(name, ret, params) ret(*name) params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
} // namespace gal

GraphicsAPI detail::graphicsAPI;

bool InitializeGraphicsAPI(GraphicsAPI api, const GraphicsAPIInitArguments& initArguments)
{
	detail::graphicsAPI = api;

	switch (api)
	{
	case GraphicsAPI::OpenGL:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::gl::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
		return eg::graphics_api::gl::Initialize(initArguments);

#ifdef EG_ENABLE_VULKAN
	case GraphicsAPI::Vulkan:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::vk::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
		gal::GetMemoryStat = &eg::graphics_api::vk::GetMemoryStat;
		return eg::graphics_api::vk::Initialize(initArguments);
#endif

#ifdef __APPLE__
	case GraphicsAPI::Metal:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::mtl::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
		return eg::graphics_api::mtl::Initialize(initArguments);
#endif

#ifdef EG_ENABLE_WEBGPU
	case GraphicsAPI::WebGPU:
#define XM_ABSCALLBACK(name, ret, params) gal::name = &graphics_api::webgpu::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
		return eg::graphics_api::webgpu::Initialize(initArguments);
#endif

	default: break;
	}

	return false;
}

bool IsGraphicsAPIMaybeAvailable(GraphicsAPI api)
{
	switch (api)
	{
	case GraphicsAPI::OpenGL: return true;

#ifdef EG_ENABLE_VULKAN
	case GraphicsAPI::Vulkan: return graphics_api::vk::EarlyInitializeMemoized();
#endif

#ifdef __APPLE__
	case GraphicsAPI::Metal: return true;
#endif

#ifdef EG_ENABLE_WEBGPU
	case GraphicsAPI::WebGPU: return graphics_api::webgpu::IsMaybeAvailable();
#endif

	default: return false;
	}
}

struct CapturedGALFunctions
{
#define XM_ABSCALLBACK(name, ret, params) ret(*name) params;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK

	void Capture()
	{
#define XM_ABSCALLBACK(name, ret, params) this->name = gal::name;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
	}
};

CapturedGALFunctions mainThreadAssertsInnerFunctions;

void InstallGraphicsAPIMainThreadAsserts()
{
	mainThreadAssertsInnerFunctions.Capture();
#define XM_ABSCALLBACK(name, ret, params)                                                                              \
	gal::name = []<typename... A>(A... args) -> ret                                                                    \
	{                                                                                                                  \
		if (!IsMainThread())                                                                                           \
		{                                                                                                              \
			EG_PANIC("Graphics function " #name " called on a background thread");                                     \
		}                                                                                                              \
		return mainThreadAssertsInnerFunctions.name(args...);                                                          \
	};

#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK

	// These functions are ok so ignore
	gal::CreateGraphicsPipeline = mainThreadAssertsInnerFunctions.CreateGraphicsPipeline;
	gal::CreateComputePipeline = mainThreadAssertsInnerFunctions.CreateComputePipeline;
	gal::CreateShaderModule = mainThreadAssertsInnerFunctions.CreateShaderModule;
	gal::CreateDescriptorSetP = mainThreadAssertsInnerFunctions.CreateDescriptorSetP;
	gal::CreateDescriptorSetB = mainThreadAssertsInnerFunctions.CreateDescriptorSetB;
	gal::BindSamplerDS = mainThreadAssertsInnerFunctions.BindSamplerDS;
	gal::BindTextureDS = mainThreadAssertsInnerFunctions.BindTextureDS;
	gal::BindStorageImageDS = mainThreadAssertsInnerFunctions.BindStorageImageDS;
	gal::BindUniformBufferDS = mainThreadAssertsInnerFunctions.BindUniformBufferDS;
	gal::BindStorageBufferDS = mainThreadAssertsInnerFunctions.BindStorageBufferDS;

	gal::GetTextureView = mainThreadAssertsInnerFunctions.GetTextureView;
}

void DestroyGraphicsAPI()
{
	gal::Shutdown();

#define XM_ABSCALLBACK(name, ret, params) gal::name = nullptr;
#include "AbstractionCallbacks.inl"
#undef XM_ABSCALLBACK
}

template <>
std::string LogToString(UniformType type)
{
	switch (type)
	{
	case UniformType::Int: return "Int";
	case UniformType::Float: return "Float";
	case UniformType::Vec2: return "Vec2";
	case UniformType::Vec3: return "Vec3";
	case UniformType::Vec4: return "Vec4";
	case UniformType::IVec2: return "IVec2";
	case UniformType::IVec3: return "IVec3";
	case UniformType::IVec4: return "IVec4";
	case UniformType::Mat3: return "Mat3";
	case UniformType::Mat4: return "Mat4";
	}
	EG_UNREACHABLE
}

std::string_view BindingTypeToString(BindingType bindingType)
{
	switch (bindingType)
	{
	case BindingType::UniformBuffer: return "UniformBuffer";
	case BindingType::StorageBuffer: return "StorageBuffer";
	case BindingType::Texture: return "Texture";
	case BindingType::StorageImage: return "StorageImage";
	case BindingType::Sampler: return "Sampler";
	}
	EG_UNREACHABLE
}

std::optional<std::variant<uint32_t, int32_t, float>> GetSpecConstantValueByID(
	std::span<const SpecializationConstantEntry> specConstants, uint32_t id)
{
	for (const SpecializationConstantEntry& entry : specConstants)
		if (entry.constantID == id)
			return entry.value;
	return std::nullopt;
}

TextureSubresource TextureSubresource::ResolveRem(uint32_t maxMipLevels, uint32_t maxArrayLayers) const
{
	TextureSubresource result = *this;
	if (numMipLevels == REMAINING_SUBRESOURCE)
		result.numMipLevels = maxMipLevels - firstMipLevel;
	if (numArrayLayers == REMAINING_SUBRESOURCE)
		result.numArrayLayers = maxArrayLayers - firstArrayLayer;
	return result;
}

TextureSubresourceLayers TextureSubresourceLayers::ResolveRem(uint32_t maxArrayLayers) const
{
	TextureSubresourceLayers result = *this;
	if (numArrayLayers == REMAINING_SUBRESOURCE)
		result.numArrayLayers = maxArrayLayers - firstArrayLayer;
	return result;
}

size_t TextureSubresource::Hash() const
{
	size_t h = 0;
	HashAppend(h, firstMipLevel);
	HashAppend(h, numMipLevels);
	HashAppend(h, firstArrayLayer);
	HashAppend(h, numArrayLayers);
	return h;
}

size_t TextureSubresourceLayers::Hash() const
{
	size_t h = 0;
	HashAppend(h, mipLevel);
	HashAppend(h, firstArrayLayer);
	HashAppend(h, numArrayLayers);
	return h;
}
} // namespace eg
