#pragma once

#include "AbstractionHL.hpp"
#include "Model.hpp"
#include "IMaterial.hpp"
#include "MeshBatch.hpp"

namespace eg
{
	class EG_API MeshBatchOrdered
	{
	public:
		MeshBatchOrdered() = default;
		
		template <typename T>
		void AddModelMesh(const Model& model, size_t meshIndex, const IMaterial& material, const T& instanceData, float order)
		{
			MeshBatch::Mesh mesh;
			mesh.vertexBuffer = model.VertexBuffer();
			mesh.indexBuffer = model.IndexBuffer();
			mesh.firstIndex = model.GetMesh(meshIndex).firstIndex;
			mesh.firstVertex = model.GetMesh(meshIndex).firstVertex;
			mesh.numElements = model.GetMesh(meshIndex).numIndices;
			mesh.indexType = model.IndexType();
			Add(mesh, material, instanceData, order);
		}
		
		template <typename T>
		void Add(const MeshBatch::Mesh& mesh, const IMaterial& material, const T& instanceData, float order)
		{
			Instance& instance = m_instances.emplace_back();
			instance.dataSize = sizeof(T);
			instance.data = m_instanceDataAllocator.New<T>(instanceData);
			instance.mesh = mesh;
			instance.material = &material;
			instance.order = order;
			m_totalInstanceData += sizeof(T);
		}
		
		void AddNoData(const MeshBatch::Mesh& mesh, const IMaterial& material, float order)
		{
			Instance& instance = m_instances.emplace_back();
			instance.dataSize = 0;
			instance.data = nullptr;
			instance.mesh = mesh;
			instance.material = &material;
			instance.order = order;
		}
		
		void Begin();
		
		void End(CommandContext& cmdCtx);
		
		void Draw(CommandContext& cmdCtx, void* drawArgs = nullptr);
		
	private:
		struct Instance
		{
			float order;
			MeshBatch::Mesh mesh;
			const IMaterial* material;
			uint32_t dataSize;
			const void* data;
		};
		
		std::vector<Instance> m_instances;
		LinearAllocator m_instanceDataAllocator;
		
		uint32_t m_totalInstanceData = 0;
		uint32_t m_instanceDataCapacity = 0;
		eg::Buffer m_instanceDataBuffer;
	};
}
