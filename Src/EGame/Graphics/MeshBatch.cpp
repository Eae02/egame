#include "MeshBatch.hpp"

namespace eg
{
	void MeshBatch::Add(const MeshBatch::Mesh& mesh, const IMaterial& material, MeshBatch::Instance* instance)
	{
		size_t pipelineHash = material.PipelineHash();
		
		//Selects a pipeline bucket
		PipelineBucket* pipelineBucket = m_drawList;
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
			pipelineBucket->next = m_drawList;
			pipelineBucket->hasInstanceData = instance->dataSize != 0;
			m_drawList = pipelineBucket;
		}
		
		//Selects a material bucket
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
		
		//Selects a model bucket
		ModelBucket* modelBucket = materialBucket->models;
		for (; modelBucket != nullptr; modelBucket = modelBucket->next)
		{
			if (modelBucket->vertexBuffer.handle == mesh.vertexBuffer.handle &&
			    modelBucket->indexBuffer.handle == mesh.indexBuffer.handle &&
			    modelBucket->indexType == mesh.indexType)
				break;
		}
		if (modelBucket == nullptr)
		{
			modelBucket = m_allocator.New<ModelBucket>();
			modelBucket->vertexBuffer = mesh.vertexBuffer;
			modelBucket->indexBuffer = mesh.indexBuffer;
			modelBucket->indexType = mesh.indexType;
			modelBucket->next = materialBucket->models;
			materialBucket->models = modelBucket;
		}
		
		//Selects a mesh bucket
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
		m_drawList = nullptr;
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
		for (PipelineBucket* pipeline = m_drawList; pipeline; pipeline =  pipeline->next)
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
		
		uploadBuffer.Unmap();
		
		if (m_totalInstanceData > m_instanceDataCapacity)
		{
			m_instanceDataCapacity = eg::RoundToNextMultiple(m_totalInstanceData, 1024);
			m_instanceDataBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer,
				m_instanceDataCapacity, nullptr);
		}
		
		cmdCtx.CopyBuffer(uploadBuffer.buffer, m_instanceDataBuffer, uploadBuffer.offset, 0, m_totalInstanceData);
		m_instanceDataBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
	}
	
	void MeshBatch::Draw(CommandContext& cmdCtx, void* drawArgs)
	{
		if (m_totalInstances == 0)
			return;
		
		for (PipelineBucket* pipeline = m_drawList; pipeline; pipeline =  pipeline->next)
		{
			pipeline->materials->material->BindPipeline(cmdCtx, drawArgs);
			
			if (pipeline->hasInstanceData)
			{
				cmdCtx.BindVertexBuffer(1, m_instanceDataBuffer, pipeline->instanceDataOffset);
			}
			
			for (MaterialBucket* material = pipeline->materials; material; material = material->next)
			{
				material->material->BindMaterial(cmdCtx, drawArgs);
				
				for (ModelBucket* model = material->models; model; model = model->next)
				{
					cmdCtx.BindVertexBuffer(0, model->vertexBuffer, 0);
					if (model->indexBuffer.handle != nullptr)
					{
						cmdCtx.BindIndexBuffer(model->indexType, model->indexBuffer, 0);
					}
					
					for (MeshBucket* mesh = model->meshes; mesh; mesh = mesh->next)
					{
						if (model->indexBuffer.handle == nullptr)
						{
							cmdCtx.Draw(mesh->firstVertex, mesh->numElements, mesh->instanceBufferOffset,
							            mesh->numInstances);
						}
						else
						{
							cmdCtx.DrawIndexed(mesh->firstIndex, mesh->numElements, mesh->firstVertex,
							                   mesh->instanceBufferOffset, mesh->numInstances);
						}
					}
				}
			}
		}
	}
}
