#include "Model.hpp"

#include <algorithm>

namespace eg
{
	void Model::Bind(CommandContext& cc, uint32_t vertexBinding) const
	{
		cc.BindVertexBuffer(vertexBinding, m_vertexBuffer, 0);
		cc.BindIndexBuffer(m_indexTypeE, m_indexBuffer, 0);
	}
	
	int Model::GetMeshIndex(std::string_view name) const
	{
		for (int i = 0; i < ToInt(m_meshes.size()); i++)
		{
			if (m_meshes[i].name == name)
				return i;
		}
		return -1;
	}
	
	int Model::RequireMeshIndex(std::string_view name) const
	{
		int idx = GetMeshIndex(name);
		if (idx != -1)
			return idx;
		std::cerr << "Mesh not found: '" << name << "', the model has the following meshes:\n";
		for (const Mesh& mesh : m_meshes)
		{
			std::cerr << " * " << mesh.name << "\n";
		}
		std::abort();
	}
	
	int Model::GetMaterialIndex(std::string_view name) const
	{
		for (int i = 0; i < ToInt(m_materialNames.size()); i++)
		{
			if (m_materialNames[i] == name)
				return i;
		}
		return -1;
	}
	
	int Model::RequireMaterialIndex(std::string_view name) const
	{
		int idx = GetMaterialIndex(name);
		if (idx != -1)
			return idx;
		std::cerr << "Material not found: '" << name << "', the model has the following materials:\n";
		for (const std::string& material : m_materialNames)
		{
			std::cerr << " * " << material << "\n";
		}
		std::abort();
	}
	
	void Model::SetAnimations(std::vector<Animation> animations)
	{
		EG_ASSERT(std::is_sorted(animations.begin(), animations.end(), AnimationNameCompare()));
		m_animations = std::move(animations);
	}
	
	const Animation* Model::FindAnimation(std::string_view name) const
	{
		auto it = std::lower_bound(m_animations.begin(), m_animations.end(), name, AnimationNameCompare());
		if (it == m_animations.end() || it->name != name)
			return nullptr;
		return &*it;
	}
	
	std::tuple<void*, void*> ModelBuilderUnformatted::AddMesh(uint32_t numVertices, uint32_t numIndices, std::string name,
		MeshAccess access, int materialIndex, const Sphere* boundingSphere, const eg::AABB* boundingAABB)
	{
		Mesh& mesh = m_meshes.emplace_back();
		mesh.access = access;
		mesh.materialIndex = materialIndex;
		mesh.numVertices = numVertices;
		mesh.numIndices = numIndices;
		mesh.name = std::move(name);
		if (boundingSphere)
			mesh.boundingSphere = *boundingSphere;
		if (boundingAABB)
			mesh.boundingAABB = *boundingAABB;
		
		char* memory = static_cast<char*>(std::malloc(numVertices * m_vertexSize + numIndices * m_indexSize));
		mesh.memory.reset(memory);
		
		return std::tuple<void*, void*>(memory, memory + numVertices * m_vertexSize);
	}
	
	int ModelBuilderUnformatted::AddMaterial(std::string_view name)
	{
		for (int i = 0; i < ToInt(m_materialNames.size()); i++)
		{
			if (m_materialNames[i] == name)
				return i;
		}
		
		int index = ToInt(m_materialNames.size());
		m_materialNames.emplace_back(name);
		return index;
	}
	
	Model ModelBuilderUnformatted::CreateAndReset()
	{
		Model model;
		model.m_vertexType = m_vertexType;
		model.m_indexType = m_indexType;
		model.m_indexTypeE = m_indexTypeE;
		model.m_materialNames.swap(m_materialNames);
		
		//Counts the amount of data to upload
		uint64_t totalVerticesBytes = 0;
		uint64_t totalIndicesBytes = 0;
		for (const Mesh& mesh : m_meshes)
		{
			if (mesh.access != MeshAccess::CPUOnly)
			{
				totalVerticesBytes += mesh.numVertices;
				totalIndicesBytes += mesh.numIndices;
			}
		}
		totalVerticesBytes *= m_vertexSize;
		totalIndicesBytes *= m_indexSize;
		
		//Uploads vertices and indices
		const uint64_t totalBytesToUpload = totalVerticesBytes + totalIndicesBytes;
		if (totalBytesToUpload != 0)
		{
			Buffer uploadBuffer(BufferFlags::HostAllocate | BufferFlags::MapWrite | BufferFlags::CopySrc, totalBytesToUpload, nullptr);
			char* verticesUploadMem = static_cast<char*>(uploadBuffer.Map(0, totalBytesToUpload));
			char* indicesUploadMem = verticesUploadMem + totalVerticesBytes;
			
			for (const Mesh& mesh : m_meshes)
			{
				if (mesh.access != MeshAccess::CPUOnly)
				{
					uint64_t meshVerticesBytes = mesh.numVertices * m_vertexSize;
					uint64_t meshIndicesBytes = mesh.numIndices * m_indexSize;
					
					const char* srcMemory = static_cast<const char*>(mesh.memory.get());
					std::memcpy(verticesUploadMem, srcMemory, meshVerticesBytes);
					std::memcpy(indicesUploadMem, srcMemory + meshVerticesBytes, meshIndicesBytes);
					verticesUploadMem += meshVerticesBytes;
					indicesUploadMem += meshIndicesBytes;
				}
			}
			
			uploadBuffer.Flush(0, totalBytesToUpload);
			
			model.m_vertexBuffer = Buffer(BufferFlags::VertexBuffer | BufferFlags::CopyDst, totalVerticesBytes, nullptr);
			model.m_indexBuffer = Buffer(BufferFlags::IndexBuffer | BufferFlags::CopyDst, totalIndicesBytes, nullptr);
			
			eg::DC.CopyBuffer(uploadBuffer, model.m_vertexBuffer, 0, 0, totalVerticesBytes);
			eg::DC.CopyBuffer(uploadBuffer, model.m_indexBuffer, totalVerticesBytes, 0, totalIndicesBytes);
			
			model.m_vertexBuffer.UsageHint(eg::BufferUsage::VertexBuffer);
			model.m_indexBuffer.UsageHint(eg::BufferUsage::IndexBuffer);
		}
		
		//Initializes the model's mesh objects
		uint32_t firstVertex = 0;
		uint32_t firstIndex = 0;
		model.m_meshes.resize(m_meshes.size());
		for (size_t i = 0; i < m_meshes.size(); i++)
		{
			model.m_meshes[i].access = m_meshes[i].access;
			model.m_meshes[i].materialIndex = m_meshes[i].materialIndex;
			model.m_meshes[i].numVertices = m_meshes[i].numVertices;
			model.m_meshes[i].numIndices = m_meshes[i].numIndices;
			model.m_meshes[i].boundingSphere = m_meshes[i].boundingSphere;
			model.m_meshes[i].boundingAABB = m_meshes[i].boundingAABB;
			model.m_meshes[i].name = std::move(m_meshes[i].name);
			
			if (m_meshes[i].access != MeshAccess::CPUOnly)
			{
				model.m_meshes[i].firstVertex = firstVertex;
				model.m_meshes[i].firstIndex = firstIndex;
				firstVertex += m_meshes[i].numVertices;
				firstIndex += m_meshes[i].numIndices;
			}
			else
			{
				model.m_meshes[i].firstVertex = UINT32_MAX;
				model.m_meshes[i].firstIndex = UINT32_MAX;
			}
			
			if (m_meshes[i].access != MeshAccess::GPUOnly)
			{
				model.m_meshes[i].memory = std::move(m_meshes[i].memory);
				model.m_meshes[i].indices = static_cast<char*>(model.m_meshes[i].memory.get()) +
					m_meshes[i].numVertices * m_vertexSize;
			}
		}
		
		m_meshes.clear();
		
		return model;
	}
}
