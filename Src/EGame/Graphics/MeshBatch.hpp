#pragma once

#include "../Alloc/LinearAllocator.hpp"
#include "AbstractionHL.hpp"
#include "IMaterial.hpp"
#include "Model.hpp"

namespace eg
{
class EG_API MeshBatch
{
public:
	struct Mesh
	{
		const MeshBuffersDescriptor* buffersDescriptor;
		uint32_t firstIndex;
		uint32_t firstVertex;

		// If buffersDescriptor->indexBuffer is nullptr this is a non-indexed draw and numElements is the number of
		// vertices to draw, otherwise it is the number of indices to draw.
		uint32_t numElements;

		static Mesh FromModel(const Model& model, size_t meshIndex)
		{
			const MeshDescriptor& meshDesc = model.GetMesh(meshIndex);
			return Mesh{
				.buffersDescriptor = &model.BuffersDescriptor(),
				.firstIndex = meshDesc.firstIndex,
				.firstVertex = meshDesc.firstVertex,
				.numElements = meshDesc.numIndices,
			};
		}
	};

	template <typename T>
	void AddModel(const Model& model, const IMaterial& material, const T& instanceData, int orderPriority = 0)
	{
		for (size_t i = 0; i < model.NumMeshes(); i++)
		{
			AddModelMesh<T>(model, i, material, instanceData, orderPriority);
		}
	}

	template <typename T>
	void AddModelMesh(
		const Model& model, size_t meshIndex, const IMaterial& material, const T& instanceData, int orderPriority = 0)
	{
		Add<T>(Mesh::FromModel(model, meshIndex), material, instanceData, orderPriority);
	}

	template <typename T>
	void Add(const Mesh& mesh, const IMaterial& material, const T& instanceData, int orderPriority = 0)
	{
		void* instanceMem = m_allocator.Allocate(sizeof(Instance) - 1 + sizeof(T), alignof(Instance));
		Instance* instance = static_cast<Instance*>(instanceMem);
		instance->dataSize = sizeof(T);
		new (instance->data) T(instanceData);
		_Add(mesh, material, instance, orderPriority, &typeid(T));
	}

	void AddNoData(const Mesh& mesh, const IMaterial& material, int orderPriority = 0)
	{
		Instance* instance = m_allocator.New<Instance>();
		instance->dataSize = 0;
		_Add(mesh, material, instance, orderPriority, nullptr);
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

	void _Add(
		const Mesh& mesh, const IMaterial& material, Instance* instance, int orderPriority,
		const std::type_info* instanceDataType);

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
		const MeshBuffersDescriptor* buffersDescriptor;
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

	struct OrderPriorityBucket
	{
		int orderPriority;
		PipelineBucket* pipelines;
	};

	std::vector<OrderPriorityBucket> m_drawList;

	eg::LinearAllocator m_allocator;
	uint32_t m_totalInstances;
	uint32_t m_totalInstanceData;

	uint32_t m_instanceDataCapacity = 0;
	eg::Buffer m_instanceDataBuffer;
};
} // namespace eg
