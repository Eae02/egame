#pragma once

#include "../../EGame/Assert.hpp"
#include "../../EGame/Utils.hpp"
#include <cstdint>
#include <string_view>
#include <vector>

namespace eg::asset_gen::gltf
{
enum class ElementType
{
	SCALAR,
	VEC2,
	VEC3,
	VEC4,
	MAT4
};

enum class ComponentType
{
	UInt8 = 5121,
	UInt16 = 5123,
	UInt32 = 5125,
	Float = 5126
};

struct BufferView
{
	int bufferIndex;
	int byteOffset;
	int byteStride;
};

struct Accessor
{
	int bufferIndex;
	int byteOffset;
	int byteStride;
	ComponentType componentType;
	int elementCount;
	ElementType elementType;
};

inline uint32_t ComponentSize(ComponentType type)
{
	switch (type)
	{
	case ComponentType::Float: return 4;
	case ComponentType::UInt8: return 1;
	case ComponentType::UInt16: return 2;
	case ComponentType::UInt32: return 4;
	}
	EG_UNREACHABLE
}

inline uint32_t ComponentsPerElement(ElementType type)
{
	switch (type)
	{
	case ElementType::SCALAR: return 1;
	case ElementType::VEC2: return 2;
	case ElementType::VEC3: return 3;
	case ElementType::VEC4: return 4;
	case ElementType::MAT4: return 16;
	}
	EG_UNREACHABLE
}

template <typename T>
inline float NormIntToFloat(T value)
{
	return static_cast<float>(value) / static_cast<float>(std::numeric_limits<T>::max());
}

inline float ReadFNormalized(const char* data, ComponentType componentType, size_t index)
{
	data += ComponentSize(componentType) * index;

	switch (componentType)
	{
	case ComponentType::Float: return *reinterpret_cast<const float*>(data);
	case ComponentType::UInt8: return NormIntToFloat(*reinterpret_cast<const uint8_t*>(data));
	case ComponentType::UInt16: return NormIntToFloat(*reinterpret_cast<const uint16_t*>(data));
	case ComponentType::UInt32: return NormIntToFloat(*reinterpret_cast<const uint32_t*>(data));
	}
	EG_UNREACHABLE
}

class GLTFData
{
public:
	inline void AddBuffer(std::vector<char> buffer) { m_buffers.push_back(std::move(buffer)); }

	inline void AddBufferView(const BufferView& view) { m_bufferViews.push_back(view); }

	inline void AddAccessor(const Accessor& accessor) { m_accessors.push_back(accessor); }

	inline const BufferView& GetBufferView(int64_t index) const
	{
		if (index < 0 || static_cast<size_t>(index) >= m_bufferViews.size())
			throw std::runtime_error("Buffer view index out of range.");
		return m_bufferViews[static_cast<size_t>(index)];
	}

	inline const Accessor& GetAccessor(int64_t index) const
	{
		if (index < 0 || static_cast<size_t>(index) >= m_accessors.size())
			throw std::runtime_error("Accessor index out of range.");
		return m_accessors[static_cast<size_t>(index)];
	}

	inline bool CheckAccessor(int64_t index, ElementType elementType, ComponentType componentType) const
	{
		return index >= 0 && static_cast<size_t>(index) < m_accessors.size() &&
		       m_accessors[static_cast<size_t>(index)].elementType == elementType &&
		       m_accessors[static_cast<size_t>(index)].componentType == componentType;
	}

	inline const char* GetAccessorData(int64_t index) const { return m_buffers[GetAccessor(index).bufferIndex].data(); }

	inline const char* GetAccessorData(const Accessor& accessor) const
	{
		return m_buffers[accessor.bufferIndex].data() + accessor.byteOffset;
	}

	static ElementType ParseElementType(std::string_view name);

private:
	std::vector<std::vector<char>> m_buffers;
	std::vector<BufferView> m_bufferViews;
	std::vector<Accessor> m_accessors;
};
} // namespace eg::asset_gen::gltf
