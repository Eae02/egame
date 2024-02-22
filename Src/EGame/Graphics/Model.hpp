#pragma once

#include "../API.hpp"
#include "../Assert.hpp"
#include "../Geometry/CollisionMesh.hpp"
#include "../Geometry/Ray.hpp"
#include "../Geometry/Sphere.hpp"
#include "AbstractionHL.hpp"
#include "Animation/Animation.hpp"
#include "Animation/Skeleton.hpp"
#include "StdVertex.hpp"

namespace eg
{
enum class ModelAccessFlags
{
	GPU = 1,
	CPU = 2,
};

EG_BIT_FIELD(ModelAccessFlags)

template <typename V, typename I>
struct MeshData
{
	std::span<V> vertices;
	std::span<I> indices;
};

struct MeshDescriptor
{
	std::string name;
	std::optional<size_t> materialIndex;
	uint32_t firstVertex;
	uint32_t firstIndex;
	uint32_t numVertices;
	uint32_t numIndices;
	std::optional<Sphere> boundingSphere;
	std::optional<eg::AABB> boundingAABB;
};

struct ModelCreateArgs
{
	ModelAccessFlags accessFlags;
	std::vector<MeshDescriptor> meshes;
	std::span<const char> vertexData;
	uint32_t numVertices;
	std::variant<std::span<const uint32_t>, std::span<const uint16_t>> indices;
	ModelVertexFormat vertexFormat;
	std::vector<std::string> materialNames;
	std::vector<Animation> animations;
	std::unique_ptr<char[]> memoryForCpuAccess;
};

struct MeshBuffersDescriptor
{
	eg::BufferRef vertexBuffer;
	eg::BufferRef indexBuffer;
	eg::IndexType indexType{};
	std::array<std::optional<uint64_t>, MAX_VERTEX_BINDINGS> vertexStreamOffsets;

	void Bind(CommandContext& cmdCtx, uint32_t enabledBindingsMask) const;
};

class EG_API Model
{
public:
	explicit Model(ModelCreateArgs createArgs);

	size_t NumMeshes() const { return m_meshes.size(); }

	const MeshDescriptor& GetMesh(size_t index) const { return m_meshes[index]; }

	size_t NumMaterials() const { return m_materialNames.size(); }

	std::variant<std::span<const uint32_t>, std::span<const uint16_t>> GetIndices() const;
	std::variant<std::span<const uint32_t>, std::span<const uint16_t>> GetMeshIndices(size_t meshIndex) const;

	std::span<const char> GetVertexData() const { return m_dataForCPUAccess->vertexData; }

	std::optional<std::pair<uint32_t, uint32_t>> GetVertexAttributeOffsetAndStride(
		ModelVertexAttributeType attributeType, uint32_t typeIndex) const;

	std::optional<std::pair<const void*, uint32_t>> GetMeshVertexAttributePtrAndStride(
		size_t meshIndex, ModelVertexAttributeType attributeType, uint32_t typeIndex) const;

	std::optional<CollisionMesh> MakeCollisionMesh(size_t meshIndex) const;

	std::optional<CollisionMesh> MakeCollisionMesh() const;

	int GetMeshIndex(std::string_view name) const;
	int RequireMeshIndex(std::string_view name) const;

	int GetMaterialIndex(std::string_view name) const;
	int RequireMaterialIndex(std::string_view name) const;

	const std::string& GetMaterialName(size_t i) const { return m_materialNames[i]; }

	const std::vector<Animation>& Animations() const { return m_animations; }

	const Animation* FindAnimation(std::string_view name) const;

	BufferRef VertexBuffer() const { return m_vertexBuffer; }

	BufferRef IndexBuffer() const { return m_indexBuffer; }

	const MeshBuffersDescriptor& BuffersDescriptor() const { return *m_buffersDescriptor; }

	const ModelVertexFormat& VertexFormat() const { return m_vertexFormat; }

	Skeleton skeleton;

private:
	size_t m_numIndices;
	uint32_t m_numVertices;

	std::vector<MeshDescriptor> m_meshes;

	std::unique_ptr<MeshBuffersDescriptor> m_buffersDescriptor;

	struct DataForCPUAccess
	{
		std::unique_ptr<char[]> meshData;
		const void* indexDataPtr;
		std::span<const char> vertexData;
	};

	std::optional<DataForCPUAccess> m_dataForCPUAccess; // nullopt if accessFlags does not include cpu

	ModelVertexFormat m_vertexFormat;

	std::vector<std::string> m_materialNames;

	Buffer m_vertexBuffer;
	Buffer m_indexBuffer;

	std::vector<Animation> m_animations;
};
} // namespace eg
