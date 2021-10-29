#pragma once

#include "AssetFormat.hpp"
#include "../Graphics/Model.hpp"
#include "../API.hpp"
#include "../IOUtils.hpp"

#include <span>

namespace eg
{
	EG_API extern const AssetFormat ModelAssetFormat;
	
	namespace detail
	{
		struct ModelVertexType
		{
			uint32_t nameHash;
			std::type_index type;
			size_t size;
			
			ModelVertexType(uint32_t _nameHash, std::type_index _type, size_t _size)
				: nameHash(_nameHash), type(_type), size(_size) { }
		};
		
		EG_API extern std::vector<ModelVertexType> modelVertexTypes;
		
		EG_API void ModelAssetWriterEnd(std::ostream& stream, const Skeleton& skeleton, std::span<const Animation> animations);
	}
	
	template <typename V>
	void DefineModelVertexType()
	{
		detail::modelVertexTypes.emplace_back(V::Name.hash, std::type_index(typeid(V)), sizeof(V));
	}
	
	EG_API bool ModelAssetLoader(const class AssetLoadContext& loadContext);
	
	EG_API MeshAccess ParseMeshAccessMode(std::string_view accessModeString, MeshAccess def = MeshAccess::GPUOnly);
	
	template <typename V>
	class ModelAssetWriter
	{
	public:
		using VertexType = V;
		
		explicit ModelAssetWriter(std::ostream& stream)
			: m_stream(&stream)
		{
			BinWrite(*m_stream, V::Name.hash);
		}
		
		void WriteMesh(std::span<const V> vertices, std::span<const uint32_t> indices, std::string_view name,
			MeshAccess access, const Sphere& boundingSphere, const AABB& boundingBox, std::string_view materialName = { })
		{
			if (vertices.empty() || indices.empty())
				EG_PANIC("Attempted to write an empty mesh.");
			
			BinWrite<uint32_t>(*m_stream, vertices.size());
			BinWrite<uint32_t>(*m_stream, indices.size());
			BinWrite<uint8_t>(*m_stream, (uint8_t)access);
			BinWriteString(*m_stream, materialName);
			BinWriteString(*m_stream, name);
			BinWrite<float>(*m_stream, boundingSphere.position.x);
			BinWrite<float>(*m_stream, boundingSphere.position.y);
			BinWrite<float>(*m_stream, boundingSphere.position.z);
			BinWrite<float>(*m_stream, boundingSphere.radius);
			BinWrite<float>(*m_stream, boundingBox.min.x);
			BinWrite<float>(*m_stream, boundingBox.min.y);
			BinWrite<float>(*m_stream, boundingBox.min.z);
			BinWrite<float>(*m_stream, boundingBox.max.x);
			BinWrite<float>(*m_stream, boundingBox.max.y);
			BinWrite<float>(*m_stream, boundingBox.max.z);
			m_stream->write(reinterpret_cast<const char*>(vertices.data()), vertices.size_bytes());
			m_stream->write(reinterpret_cast<const char*>(indices.data()), indices.size_bytes());
		}
		
		void End()
		{
			detail::ModelAssetWriterEnd(*m_stream, Skeleton(), {});
		}
		
		void End(const Skeleton& skeleton, std::span<const Animation> animations)
		{
			detail::ModelAssetWriterEnd(*m_stream, skeleton, animations);
		}
		
	private:
		std::ostream* m_stream;
	};
}
