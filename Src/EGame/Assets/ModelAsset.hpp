#pragma once

#include "AssetFormat.hpp"
#include "../Graphics/Model.hpp"
#include "../API.hpp"
#include "../IOUtils.hpp"

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
	}
	
	template <typename V>
	void DefineModelVertexType()
	{
		detail::modelVertexTypes.emplace_back(V::Name.hash, std::type_index(typeid(V)), sizeof(V));
	}
	
	EG_API bool ModelAssetLoader(const class AssetLoadContext& loadContext);
	
	template <typename V>
	class ModelAssetWriter
	{
	public:
		explicit ModelAssetWriter(std::ostream& stream)
			: m_stream(&stream)
		{
			BinWrite(*m_stream, V::Name.hash);
		}
		
		void WriteMesh(Span<const V> vertices, Span<const uint32_t> indices, std::string_view name,
			MeshAccess access, int materialIndex = -1)
		{
			if (vertices.Empty() || indices.Empty())
				EG_PANIC("Attempted to write an empty mesh.");
			
			BinWrite<uint32_t>(*m_stream, vertices.size());
			BinWrite<uint32_t>(*m_stream, indices.size());
			BinWrite<int32_t>(*m_stream, materialIndex);
			BinWrite<uint8_t>(*m_stream, (uint8_t)access);
			BinWriteString(*m_stream, name);
			m_stream->write(reinterpret_cast<const char*>(vertices.data()), vertices.SizeBytes());
			m_stream->write(reinterpret_cast<const char*>(indices.data()), indices.SizeBytes());
		}
		
		void End()
		{
			BinWrite<uint32_t>(*m_stream, 0);
			BinWrite<uint32_t>(*m_stream, 0);
		}
		
	private:
		std::ostream* m_stream;
	};
}
