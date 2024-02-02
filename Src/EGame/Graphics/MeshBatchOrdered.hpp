#pragma once

#include "AbstractionHL.hpp"
#include "IMaterial.hpp"
#include "MeshBatch.hpp"

namespace eg
{
class EG_API MeshBatchOrdered
{
public:
	MeshBatchOrdered() = default;

	template <typename T>
	void AddModel(const Model& model, const IMaterial& material, const T& instanceData, float order)
	{
		for (size_t i = 0; i < model.NumMeshes(); i++)
		{
			AddModelMesh<T>(model, i, material, instanceData, order);
		}
	}

	template <typename T>
	void AddModelMesh(
		const class Model& model, size_t meshIndex, const IMaterial& material, const T& instanceData, float order)
	{
		_AddModelMesh(
			model, meshIndex, material, m_instanceDataAllocator.New<T>(instanceData), sizeof(T), order, typeid(T));
	}

	template <typename T>
	void Add(const MeshBatch::Mesh& mesh, const IMaterial& material, const T& instanceData, float order)
	{
		_Add(mesh, material, m_instanceDataAllocator.New<T>(instanceData), sizeof(T), order, typeid(T));
	}

	void AddNoData(const MeshBatch::Mesh& mesh, const IMaterial& material, float order);

	void Begin();

	void End(CommandContext& cmdCtx);

	void Draw(CommandContext& cmdCtx, void* drawArgs = nullptr) const;

private:
	void _AddModelMesh(
		const class Model& model, size_t meshIndex, const IMaterial& material, const void* data, uint32_t dataSize,
		float order, const std::type_info& instanceDataType);
	void _Add(
		const MeshBatch::Mesh& mesh, const IMaterial& material, const void* data, uint32_t dataSize, float order,
		const std::type_info& instanceDataType);

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
} // namespace eg
