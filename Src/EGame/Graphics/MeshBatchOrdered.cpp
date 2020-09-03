#include "MeshBatchOrdered.hpp"
#include "Model.hpp"

namespace eg
{
	void MeshBatchOrdered::Begin()
	{
		m_instances.clear();
		m_instanceDataAllocator.Reset();
		m_totalInstanceData = 0;
	}
	
	static inline void CheckRequirements(const IMaterial& material, const std::type_info* instanceDataType)
	{
		if (material.GetOrderRequirement() == IMaterial::OrderRequirement::OnlyUnordered)
		{
			EG_PANIC("Attempted to add a material with order requirement OnlyUnordered to an ordered mesh batch.");
		}
		
		if (!material.CheckInstanceDataType(instanceDataType))
		{
			const char* instanceDataTypeName = instanceDataType ? instanceDataType->name() : "none";
			EG_PANIC("Attempted to use incompatible instance data type (" << instanceDataTypeName << ")");
		}
	}
	
	void MeshBatchOrdered::AddNoData(const MeshBatch::Mesh& mesh, const IMaterial& material, float order)
	{
		CheckRequirements(material, nullptr);
		Instance& instance = m_instances.emplace_back();
		instance.dataSize = 0;
		instance.data = nullptr;
		instance.mesh = mesh;
		instance.material = &material;
		instance.order = order;
	}
	
	void MeshBatchOrdered::_AddModelMesh(const Model& model, size_t meshIndex, const IMaterial& material,
			const void* data, size_t dataSize, float order, const std::type_info& instanceDataType)
	{
		CheckRequirements(material, &instanceDataType);
		
		Instance& instance = m_instances.emplace_back();
		instance.dataSize = dataSize;
		instance.data = data;
		instance.mesh.vertexBuffer = model.VertexBuffer();
		instance.mesh.indexBuffer = model.IndexBuffer();
		instance.mesh.firstIndex = model.GetMesh(meshIndex).firstIndex;
		instance.mesh.firstVertex = model.GetMesh(meshIndex).firstVertex;
		instance.mesh.numElements = model.GetMesh(meshIndex).numIndices;
		instance.mesh.indexType = model.IndexType();
		instance.material = &material;
		instance.order = order;
		m_totalInstanceData += dataSize;
	}
	
	void MeshBatchOrdered::_Add(const MeshBatch::Mesh& mesh, const IMaterial& material, const void* data,
			size_t dataSize, float order, const std::type_info& instanceDataType)
	{
		CheckRequirements(material, &instanceDataType);
		
		Instance& instance = m_instances.emplace_back();
		instance.dataSize = dataSize;
		instance.data = data;
		instance.mesh = mesh;
		instance.material = &material;
		instance.order = order;
		m_totalInstanceData += dataSize;
	}
	
	void MeshBatchOrdered::End(CommandContext& cmdCtx)
	{
		if (m_instances.empty())
			return;
		
		std::sort(m_instances.begin(), m_instances.end(), [&] (const Instance& a, const Instance& b)
		{
			return a.order < b.order;
		});
		
		if (m_totalInstanceData != 0)
		{
			eg::UploadBuffer uploadBuffer = eg::GetTemporaryUploadBuffer(m_totalInstanceData);
			
			char* instanceDataOut = static_cast<char*>(uploadBuffer.Map());
			for (const Instance& instance : m_instances)
			{
				std::memcpy(instanceDataOut, instance.data, instance.dataSize);
				instanceDataOut += instance.dataSize;
			}
			
			uploadBuffer.Flush();
			
			if (m_totalInstanceData > m_instanceDataCapacity)
			{
				m_instanceDataCapacity = eg::RoundToNextMultiple(m_totalInstanceData, 1024);
				m_instanceDataBuffer = eg::Buffer(eg::BufferFlags::CopyDst | eg::BufferFlags::VertexBuffer,
					m_instanceDataCapacity, nullptr);
			}
			
			cmdCtx.CopyBuffer(uploadBuffer.buffer, m_instanceDataBuffer, uploadBuffer.offset, 0, m_totalInstanceData);
			m_instanceDataBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
		}
	}
	
	void MeshBatchOrdered::Draw(CommandContext& cmdCtx, void* drawArgs) const
	{
		if (m_instances.empty())
			return;
		
		const IMaterial* currentMaterial = nullptr;
		size_t currentPipelineHash = 0;
		
		uint32_t instanceDataOffset = 0;
		
		for (const Instance& instance : m_instances)
		{
			size_t newPipelineHash = instance.material->PipelineHash();
			if (currentMaterial == nullptr || newPipelineHash != currentPipelineHash)
			{
				if (!instance.material->BindPipeline(cmdCtx, drawArgs))
				{
					instanceDataOffset += instance.dataSize;
					continue;
				}
				currentPipelineHash = newPipelineHash;
			}
			
			if (currentMaterial != instance.material)
			{
				if (!instance.material->BindMaterial(cmdCtx, drawArgs))
				{
					instanceDataOffset += instance.dataSize;
					continue;
				}
				currentMaterial = instance.material;
			}
			
			if (instance.dataSize != 0)
			{
				cmdCtx.BindVertexBuffer(1, m_instanceDataBuffer, instanceDataOffset);
				instanceDataOffset += instance.dataSize;
			}
			
			cmdCtx.BindVertexBuffer(0, instance.mesh.vertexBuffer, 0);
			if (instance.mesh.indexBuffer.handle == nullptr)
			{
				cmdCtx.Draw(instance.mesh.firstVertex, instance.mesh.numElements, 0, 1);
			}
			else
			{
				cmdCtx.BindIndexBuffer(instance.mesh.indexType, instance.mesh.indexBuffer, 0);
				
				cmdCtx.DrawIndexed(instance.mesh.firstIndex, instance.mesh.numElements, instance.mesh.firstVertex, 0, 1);
			}
		}
	}
}
