#pragma once

#include "AbstractionHL.hpp"
#include "StdVertex.hpp"
#include "Animation/Animation.hpp"
#include "Animation/Skeleton.hpp"
#include "../CollisionMesh.hpp"
#include "../API.hpp"
#include "../Ray.hpp"
#include "../Sphere.hpp"

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
		std::span<V> vertices;
		std::span<I> indices;
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
			std::optional<Sphere> boundingSphere;
			std::optional<eg::AABB> boundingAABB;
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
		
		size_t NumMaterials() const
		{
			return m_materialNames.size();
		}
		
		template <typename V = StdVertex, typename I = uint32_t>
		MeshData<const V, const I> GetMeshData(size_t index) const
		{
			if (index >= m_meshes.size())
			{
				EG_PANIC("Attempted to get mesh data for an invalid mesh index.");
			}
			if (m_meshes[index].access == MeshAccess::GPUOnly)
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
			data.vertices = std::span<const V>(static_cast<const V*>(m_meshes[index].memory.get()), m_meshes[index].numVertices);
			data.indices = std::span<const I>(static_cast<const I*>(m_meshes[index].indices), m_meshes[index].numIndices);
			return data;
		}
		
		template <typename V = StdVertex, typename I = uint32_t>
		CollisionMesh MakeCollisionMesh(size_t index) const
		{
			MeshData<const V, const I> data = GetMeshData<V, I>(index);
			return CollisionMesh::Create<V, I>(data.vertices, data.indices);
		}
		
		template <typename V = StdVertex, typename I = uint32_t>
		CollisionMesh MakeCollisionMesh() const
		{
			if (m_meshes.size() == 0)
				return CollisionMesh();
			if (m_meshes.size() == 1)
				return MakeCollisionMesh(0);
			std::vector<CollisionMesh> meshes(m_meshes.size());
			for (size_t i = 0; i < meshes.size(); i++)
			{
				meshes[i] = MakeCollisionMesh<V, I>(i);
			}
			return CollisionMesh::Join(meshes);
		}
		
		void Bind(CommandContext& cc = DC, uint32_t vertexBinding = 0) const;
		
		int GetMeshIndex(std::string_view name) const;
		int RequireMeshIndex(std::string_view name) const;
		
		int GetMaterialIndex(std::string_view name) const;
		int RequireMaterialIndex(std::string_view name) const;
		
		const std::string& GetMaterialName(size_t i) const { return m_materialNames[i]; }
		
		const std::vector<Animation>& Animations() const
		{
			return m_animations;
		}
		
		void SetAnimations(std::vector<Animation> animations);
		
		const Animation* FindAnimation(std::string_view name) const;
		
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
		
		std::type_index VertexType() const
		{
			return m_vertexType;
		}
		
		Skeleton skeleton;
		
	private:
		struct InternalMesh : Mesh
		{
			std::unique_ptr<void, FreeDel> memory;
			void* indices;
		};
		
		std::vector<InternalMesh> m_meshes;
		
		std::vector<std::string> m_materialNames;
		
		std::type_index m_vertexType { typeid(int) };
		std::type_index m_indexType { typeid(int) };
		eg::IndexType m_indexTypeE;
		
		Buffer m_vertexBuffer;
		Buffer m_indexBuffer;
		
		std::vector<Animation> m_animations;
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
		
		int AddMaterial(std::string_view name);
		
		std::tuple<void*, void*> AddMesh(uint32_t numVertices, uint32_t numIndices,
			std::string name, MeshAccess access = MeshAccess::All,
			int materialIndex = -1, const Sphere* boundingSphere = nullptr, const eg::AABB* boundingAABB = nullptr);
		
	private:
		struct Mesh
		{
			MeshAccess access;
			int materialIndex;
			uint32_t numVertices;
			uint32_t numIndices;
			std::string name;
			std::optional<Sphere> boundingSphere;
			std::optional<eg::AABB> boundingAABB;
			std::unique_ptr<void, FreeDel> memory;
		};
		
		std::vector<std::string> m_materialNames;
		
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
		
		MeshData<V, I> AddMesh(uint32_t numVertices, uint32_t numIndices, std::string name, MeshAccess access = MeshAccess::All,
			int materialIndex = -1, const Sphere* boundingSphere = nullptr, const eg::AABB* boundingAABB = nullptr)
		{
			auto [vertices, indices] = m_builder.AddMesh(numVertices, numIndices,
				std::move(name), access, materialIndex, boundingSphere, boundingAABB);
			
			MeshData<V, I> data;
			data.vertices = std::span<V>(reinterpret_cast<V*>(vertices), numVertices);
			data.indices = std::span<I>(reinterpret_cast<I*>(indices), numIndices);
			return data;
		}
		
		Model CreateAndReset()
		{
			return m_builder.CreateAndReset();
		}
		
	private:
		ModelBuilderUnformatted m_builder;
	};
	
	EG_API void GenerateTangents(std::span<const uint32_t> indices, size_t numVertices, const glm::vec3* positions,
		const glm::vec2* texCoords, const glm::vec3* normals, glm::vec3* tangentsOut);
}
