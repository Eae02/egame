#include "MeshBatch.hpp"
#include "../Assert.hpp"

#include <algorithm>

namespace eg
{
void MeshBatch::_Add(
	const MeshBatch::Mesh& mesh, const IMaterial& material, MeshBatch::Instance* instance, int orderPriority,
	const std::type_info* instanceDataType)
{
	if (material.GetOrderRequirement() == IMaterial::OrderRequirement::OnlyOrdered)
	{
		EG_PANIC("Attempted to add a material with order requirement OnlyOrdered to an unordered mesh batch.");
	}

	if (!material.CheckInstanceDataType(instanceDataType))
	{
		const char* instanceDataTypeName = instanceDataType ? instanceDataType->name() : "none";
		EG_PANIC("Attempted to use incompatible instance data type (" << instanceDataTypeName << ")");
	}

	size_t pipelineHash = material.PipelineHash();

	auto opBucketIt = std::lower_bound(
		m_drawList.begin(), m_drawList.end(), orderPriority,
		[&](const OrderPriorityBucket& a, int b) { return a.orderPriority < b; });
	if (opBucketIt == m_drawList.end() || opBucketIt->orderPriority != orderPriority)
	{
		opBucketIt = m_drawList.emplace(opBucketIt);
		opBucketIt->orderPriority = orderPriority;
		opBucketIt->pipelines = nullptr;
	}

	// Selects a pipeline bucket
	PipelineBucket* pipelineBucket = opBucketIt->pipelines;
	for (; pipelineBucket != nullptr; pipelineBucket = pipelineBucket->next)
	{
		if (pipelineBucket->pipelineHash == pipelineHash)
			break;
	}
	if (pipelineBucket == nullptr)
	{
		pipelineBucket = m_allocator.New<PipelineBucket>();
		pipelineBucket->pipelineHash = pipelineHash;
		pipelineBucket->materials = nullptr;
		pipelineBucket->next = opBucketIt->pipelines;
		pipelineBucket->hasInstanceData = instance->dataSize != 0;
		opBucketIt->pipelines = pipelineBucket;
	}

	// Selects a material bucket
	MaterialBucket* materialBucket = pipelineBucket->materials;
	for (; materialBucket != nullptr; materialBucket = materialBucket->next)
	{
		if (materialBucket->material == &material)
			break;
	}
	if (materialBucket == nullptr)
	{
		materialBucket = m_allocator.New<MaterialBucket>();
		materialBucket->material = &material;
		materialBucket->models = nullptr;
		materialBucket->next = pipelineBucket->materials;
		pipelineBucket->materials = materialBucket;
	}

	// Selects a model bucket
	ModelBucket* modelBucket = materialBucket->models;
	for (; modelBucket != nullptr; modelBucket = modelBucket->next)
	{
		if (modelBucket->buffersDescriptor == mesh.buffersDescriptor)
			break;
	}
	if (modelBucket == nullptr)
	{
		modelBucket = m_allocator.New<ModelBucket>();
		modelBucket->buffersDescriptor = mesh.buffersDescriptor;
		modelBucket->next = materialBucket->models;
		materialBucket->models = modelBucket;
	}

	// Selects a mesh bucket
	MeshBucket* meshBucket = modelBucket->meshes;
	for (; meshBucket != nullptr; meshBucket = meshBucket->next)
	{
		if (meshBucket->firstIndex == mesh.firstIndex && meshBucket->firstVertex == mesh.firstVertex &&
		    meshBucket->numElements == mesh.numElements)
			break;
	}
	if (meshBucket == nullptr)
	{
		meshBucket = m_allocator.New<MeshBucket>();
		meshBucket->firstIndex = mesh.firstIndex;
		meshBucket->firstVertex = mesh.firstVertex;
		meshBucket->numElements = mesh.numElements;
		meshBucket->next = modelBucket->meshes;
		modelBucket->meshes = meshBucket;
	}

	instance->next = nullptr;

	if (meshBucket->firstInstance == nullptr)
	{
		meshBucket->firstInstance = meshBucket->lastInstance = instance;
	}
	else if (meshBucket->lastInstance->dataSize != instance->dataSize)
	{
		EG_PANIC("Instance data size mismatch when using the same material");
	}
	else
	{
		meshBucket->lastInstance->next = instance;
		meshBucket->lastInstance = instance;
	}

	meshBucket->numInstances++;
	m_totalInstanceData += instance->dataSize;
	m_totalInstances++;
}

void MeshBatch::Begin()
{
	m_allocator.Reset();
	m_drawList.clear();
	m_totalInstances = 0;
	m_totalInstanceData = 0;
}

void MeshBatch::End(CommandContext& cmdCtx)
{
	if (m_totalInstances == 0)
		return;

	eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(m_totalInstanceData);

	char* instanceDataOut = static_cast<char*>(uploadBuffer.Map());
	uint32_t instanceDataOffset = 0;
	for (const OrderPriorityBucket& opBucket : m_drawList)
	{
		for (PipelineBucket* pipeline = opBucket.pipelines; pipeline; pipeline = pipeline->next)
		{
			pipeline->instanceDataOffset = instanceDataOffset;
			uint32_t instanceIndex = 0;
			for (MaterialBucket* material = pipeline->materials; material; material = material->next)
			{
				for (ModelBucket* model = material->models; model; model = model->next)
				{
					for (MeshBucket* mesh = model->meshes; mesh; mesh = mesh->next)
					{
						mesh->instanceBufferOffset = instanceIndex;
						for (Instance* instance = mesh->firstInstance; instance; instance = instance->next)
						{
							std::memcpy(instanceDataOut + instanceDataOffset, instance->data, instance->dataSize);
							instanceDataOffset += instance->dataSize;
						}
						instanceIndex += mesh->numInstances;
					}
				}
			}
		}
	}

	uploadBuffer.Flush();

	if (m_totalInstanceData > m_instanceDataCapacity)
	{
		m_instanceDataCapacity = eg::RoundToNextMultiple(m_totalInstanceData, 1024);
		m_instanceDataBuffer =
			eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer, m_instanceDataCapacity, nullptr);
	}

	cmdCtx.CopyBuffer(uploadBuffer.buffer, m_instanceDataBuffer, uploadBuffer.offset, 0, m_totalInstanceData);
	m_instanceDataBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
}

void MeshBatch::Draw(CommandContext& cmdCtx, void* drawArgs)
{
	if (m_totalInstances == 0)
		return;

	for (const OrderPriorityBucket& opBucket : m_drawList)
	{
		for (PipelineBucket* pipeline = opBucket.pipelines; pipeline; pipeline = pipeline->next)
		{
			if (!pipeline->materials->material->BindPipeline(cmdCtx, drawArgs))
				continue;

			auto vertexInputConfig = pipeline->materials->material->GetVertexInputConfiguration(drawArgs);
			EG_ASSERT(vertexInputConfig.instanceDataBindingIndex.has_value() == pipeline->hasInstanceData);

			if (pipeline->hasInstanceData)
			{
				cmdCtx.BindVertexBuffer(
					*vertexInputConfig.instanceDataBindingIndex, m_instanceDataBuffer, pipeline->instanceDataOffset);
			}

			for (MaterialBucket* material = pipeline->materials; material; material = material->next)
			{
				if (!material->material->BindMaterial(cmdCtx, drawArgs))
					continue;

				for (ModelBucket* model = material->models; model; model = model->next)
				{
					const MeshBuffersDescriptor& buffersDescriptor = *model->buffersDescriptor;

					buffersDescriptor.Bind(cmdCtx, vertexInputConfig.vertexBindingsMask);

					for (MeshBucket* mesh = model->meshes; mesh; mesh = mesh->next)
					{
						if (buffersDescriptor.indexBuffer.handle != nullptr) [[likely]]
						{
							cmdCtx.DrawIndexed(
								mesh->firstIndex, mesh->numElements, mesh->firstVertex, mesh->instanceBufferOffset,
								mesh->numInstances);
						}
						else
						{
							cmdCtx.Draw(
								mesh->firstVertex, mesh->numElements, mesh->instanceBufferOffset, mesh->numInstances);
						}
					}
				}
			}
		}
	}
}
} // namespace eg
