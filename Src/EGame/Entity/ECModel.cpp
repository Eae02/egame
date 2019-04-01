#include "ECModel.hpp"
#include "EntityManager.hpp"
#include "ECTransform.hpp"

#include "../Graphics/Model.hpp"
#include "../Graphics/MeshBatch.hpp"

namespace eg
{
	static EntitySignature modelSignature = EntitySignature::Create<ECModel>();
	
	void ECModel::Render(EntityManager& entityManager, MeshBatch& meshBatch, uint32_t modeMask)
	{
		for (const Entity& entity : entityManager.GetEntitySet(modelSignature))
		{
			const ECModel* model = entity.GetComponent<ECModel>();
			if ((model->m_modeMask & modeMask) != modeMask)
				continue;
			
			glm::mat4 transform = GetEntityTransform3D(entity);
			
			for (size_t i = 0; i < model->m_model->NumMeshes(); i++)
			{
				if (const IMaterial* material = model->m_materials[model->m_model->GetMesh(i).materialIndex])
				{
					meshBatch.Add(*model->m_model, i, *material, transform * model->m_meshTransforms[i]);
				}
			}
		}
	}
	
	void ECModel::SetModel(const Model* model)
	{
		m_model = model;
		m_materials.resize(model->NumMaterials());
		std::fill(m_materials.begin(), m_materials.end(), nullptr);
		m_meshTransforms.resize(model->NumMeshes());
		std::fill(m_meshTransforms.begin(), m_meshTransforms.end(), glm::mat4(1.0f));
	}
	
	void ECModel::SetMaterial(std::string_view name, const IMaterial* material)
	{
		int index = m_model->GetMaterialIndex(name);
		if (index == -1)
			EG_PANIC("Material not found: '" << name << "'.");
		SetMaterial(index, material);
	}
}
