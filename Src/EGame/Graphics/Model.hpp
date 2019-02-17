#pragma once

#include "AbstractionHL.hpp"
#include "../Span.hpp"
#include "../API.hpp"

namespace eg
{
	enum class MeshAccess
	{
		GPUOnly,
		CPUOnly,
		All
	};
	
	template <typename V, typename I>
	struct MeshData
	{
		Span<V> vertices;
		Span<I> indices;
	};
	
	class EG_API Model
	{
	public:
		friend class ModelBuilderUnformatted;
		
		struct Mesh
		{
			std::string name;
			MeshAccess access;
			int materialIndex;
			uint32_t firstVertex;
			uint32_t firstIndex;
			uint32_t numVertices;
			uint32_t numIndices;
		};
		
		Model() = default;
		
		size_t NumMeshes() const
		{
			return m_meshes.size();
		}
		
		const Mesh& GetMesh(size_t index) const
		{
			return m_meshes[index];
		}
		
		template <typename V, typename I>
		MeshData<const V, const I> GetMeshData(size_t index) const
		{
			if (m_meshes[index].access == MeshAccess::CPUOnly)
			{
				EG_PANIC("Attempted to get mesh data for a mesh which is not CPU accessible.");
			}
			
			if (m_vertexType != std::type_index(typeid(V)))
			{
				EG_PANIC("Attempted to get mesh data with an incorrect vertex type.");
			}
			
			if (m_indexType != std::type_index(typeid(I)))
			{
				EG_PANIC("Attempted to get mesh data with an incorrect index type.");
			}
			
			MeshData<const V, const I> data;
			data.vertices = Span<const V>(static_cast<const V*>(m_meshes[index].memory.get()), m_meshes[index].numVertices);
			data.indices = Span<const I>(static_cast<const I*>(m_meshes[index].indices), m_meshes[index].numIndices);
			return data;
		}
		
		void Bind(CommandContext& cc = DC, uint32_t vertexBinding = 0) const;
		
		BufferRef VertexBuffer() const
		{
			return m_vertexBuffer;
		}
		
		BufferRef IndexBuffer() const
		{
			return m_indexBuffer;
		}
		
		eg::IndexType IndexType() const
		{
			return m_indexTypeE;
		}
		
	private:
		struct InternalMesh : Mesh
		{
			std::unique_ptr<void, FreeDel> memory;
			void* indices;
		};
		
		std::vector<InternalMesh> m_meshes;
		
		std::type_index m_vertexType { typeid(int) };
		std::type_index m_indexType { typeid(int) };
		eg::IndexType m_indexTypeE;
		
		Buffer m_vertexBuffer;
		Buffer m_indexBuffer;
	};
	
	class EG_API ModelBuilderUnformatted
	{
	public:
		ModelBuilderUnformatted(std::type_index vertexType, uint64_t vertexSize,
			std::type_index indexType, uint64_t indexSize, IndexType indexTypeE)
			: m_vertexSize(vertexSize), m_indexSize(indexSize),
			  m_vertexType(vertexType), m_indexType(indexType), m_indexTypeE(indexTypeE) { }
		
		ModelBuilderUnformatted(ModelBuilderUnformatted&&) = default;
		ModelBuilderUnformatted& operator=(ModelBuilderUnformatted&&) = default;
		ModelBuilderUnformatted(const ModelBuilderUnformatted&) = delete;
		ModelBuilderUnformatted& operator=(const ModelBuilderUnformatted&) = delete;
		
		/**
		 * Creates the model, this will also reset the model builder.
		 */
		Model CreateAndReset();
		
		std::tuple<void*, void*> AddMesh(uint32_t numVertices, uint32_t numIndices,
			std::string name, MeshAccess access = MeshAccess::All, int materialIndex = -1);
		
	private:
		struct Mesh
		{
			MeshAccess access;
			int materialIndex;
			uint32_t numVertices;
			uint32_t numIndices;
			std::string name;
			std::unique_ptr<void, FreeDel> memory;
		};
		
		uint64_t m_vertexSize;
		uint64_t m_indexSize;
		std::type_index m_vertexType;
		std::type_index m_indexType;
		IndexType m_indexTypeE;
		
		std::vector<Mesh> m_meshes;
	};
	
	template <typename V, typename I>
	class ModelBuilder
	{
	public:
		ModelBuilder()
			: m_builder(std::type_index(typeid(V)), sizeof(V), std::type_index(typeid(I)), sizeof(I), GetIndexType<I>()) { }
		
		MeshData<V, I> AddMesh(uint32_t numVertices, uint32_t numIndices, std::string name,
			MeshAccess access = MeshAccess::All, int materialIndex = -1)
		{
			auto [vertices, indices] = m_builder.AddMesh(numVertices, numIndices,
				std::move(name), access, materialIndex);
			
			MeshData<V, I> data;
			data.vertices = Span<V>(reinterpret_cast<V*>(vertices), numVertices);
			data.indices = Span<I>(reinterpret_cast<I*>(indices), numIndices);
			return data;
		}
		
		Model CreateAndReset()
		{
			return m_builder.CreateAndReset();
		}
		
	private:
		ModelBuilderUnformatted m_builder;
	};
	
	EG_API void GenerateTangents(Span<const uint32_t> indices, size_t numVertices, const glm::vec3* positions,
		const glm::vec2* texCoords, const glm::vec3* normals, glm::vec3* tangentsOut);
}
