#pragma once

#include "../API.hpp"

namespace eg
{
	class EG_API ECModel
	{
	public:
		ECModel() = default;
		
		static void Render(class EntityManager& entityManager, class MeshBatch& meshBatch, uint32_t modeMask = 0);
		
		uint32_t ModeMask() const
		{
			return m_modeMask;
		}
		
		void SetModeMask(uint32_t modeMask)
		{
			m_modeMask = modeMask;
		}
		
		const class Model* GetModel() const
		{
			return m_model;
		}
		
		void SetModel(const class Model* model);
		
		void SetMeshTransform(size_t index, const glm::mat4& transform)
		{
			m_meshTransforms.at(index) = transform;
		}
		
		const glm::mat4& GetMeshTransform(size_t index) const
		{
			return m_meshTransforms.at(index);
		}
		
		void SetMaterial(const class IMaterial* material)
		{
			std::fill(m_materials.begin(), m_materials.end(), material);
		}
		
		void SetMaterial(std::string_view name, const class IMaterial* material);
		
		void SetMaterial(size_t index, const class IMaterial* material)
		{
			m_materials.at(index) = material;
		}
		
		const class IMaterial* GetMaterial(size_t index) const
		{
			return m_materials.at(index);
		}
		
	private:
		uint32_t m_modeMask = UINT32_MAX;
		const class Model* m_model = nullptr;
		std::vector<const class IMaterial*> m_materials;
		std::vector<glm::mat4> m_meshTransforms;
	};
}
