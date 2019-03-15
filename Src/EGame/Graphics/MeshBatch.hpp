#pragma once

#include "AbstractionHL.hpp"
#include "Model.hpp"
#include "../API.hpp"

namespace eg
{
	class EG_API IMaterial
	{
	public:
		virtual size_t PipelineHash() const = 0;
		virtual void BindPipeline(CommandContext& cmdCtx, void* drawArgs) const = 0;
		virtual void BindMaterial(CommandContext& cmdCtx, void* drawArgs) const = 0;
	};
	
	class EG_API MeshBatch
	{
	public:
		struct Mesh
		{
			eg::BufferRef vertexBuffer;
			eg::BufferRef indexBuffer;
			uint32_t firstIndex;
			uint32_t firstVertex;
			uint32_t numElements; //Number of vertices or indices
			eg::IndexType indexType;
		};
		
		template <typename T>
		void Add(const Model& model, const IMaterial& material, const T& instanceData)
		{
			for (size_t i = 0; i < model.NumMeshes(); i++)
			{
				Add(model, i, material, instanceData);
			}
		}
		
		template <typename T>
		void Add(const Model& model, size_t meshIndex, const IMaterial& material, const T& instanceData)
		{
			Mesh mesh;
			mesh.vertexBuffer = model.VertexBuffer();
			mesh.indexBuffer = model.IndexBuffer();
			mesh.firstIndex = model.GetMesh(meshIndex).firstIndex;
			mesh.firstVertex = model.GetMesh(meshIndex).firstVertex;
			mesh.numElements = model.GetMesh(meshIndex).numIndices;
			mesh.indexType = model.IndexType();
			Add(mesh, material, instanceData);
		}
		
		template <typename T>
		void Add(const Mesh& mesh, const IMaterial& material, const T& instanceData)
		{
			void* instanceMem = m_allocator.Allocate(sizeof(Instance) - 1 + sizeof(T), alignof(Instance));
			Instance* instance = static_cast<Instance*>(instanceMem);
			instance->dataSize = sizeof(T);
			new (instance->data) T (instanceData);
			Add(mesh, material, instance);
		}
		
		void Add(const Mesh& mesh, const IMaterial& material)
		{
			Instance* instance = m_allocator.New<Instance>();
			instance->dataSize = 0;
			Add(mesh, material, instance);
		}
		
		void Begin();
		
		void End(CommandContext& cmdCtx);
		
		void Draw(CommandContext& cmdCtx, void* drawArgs = nullptr);
		
	private:
		struct Instance
		{
			Instance* next;
			uint32_t dataSize;
			alignas(std::max_align_t) char data[1];
		};
		
		void Add(const Mesh& mesh, const IMaterial& material, Instance* instance);
		
		struct MeshBucket
		{
			uint32_t firstVertex;
			uint32_t firstIndex;
			uint32_t numElements;
			Instance* firstInstance = nullptr;
			Instance* lastInstance = nullptr;
			uint32_t numInstances = 0;
			uint32_t instanceBufferOffset = 0;
			MeshBucket* next = nullptr;
		};
		
		struct ModelBucket
		{
			eg::BufferRef vertexBuffer;
			eg::BufferRef indexBuffer;
			eg::IndexType indexType;
			MeshBucket* meshes;
			ModelBucket* next = nullptr;
		};
		
		struct MaterialBucket
		{
			const IMaterial* material;
			ModelBucket* models;
			MaterialBucket* next;
		};
		
		struct PipelineBucket
		{
			size_t pipelineHash;
			MaterialBucket* materials;
			PipelineBucket* next;
			uint32_t instanceDataOffset;
			bool hasInstanceData;
		};
		
		eg::LinearAllocator m_allocator;
		PipelineBucket* m_drawList;
		uint32_t m_totalInstances;
		uint32_t m_totalInstanceData;
		
		uint32_t m_instanceDataCapacity = 0;
		eg::Buffer m_instanceDataBuffer;
	};
}
