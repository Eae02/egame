#pragma once

#include "../API.hpp"
#include "../Assert.hpp"
#include "../Graphics/Model.hpp"
#include "../Graphics/ModelVertexFormat.hpp"
#include "../IOUtils.hpp"
#include "AssetFormat.hpp"

#include <cstdint>
#include <span>

namespace eg
{
EG_API extern const AssetFormat ModelAssetFormat;

EG_API bool ModelAssetLoader(const class AssetLoadContext& loadContext);

EG_API ModelAccessFlags ParseModelAccessFlagsMode(std::string_view accessModeString, ModelAccessFlags def);

struct WriteModelAssetMesh
{
	std::span<const glm::vec3> positions;
	std::span<const glm::vec3> normals;
	std::span<const glm::vec3> tangents;
	std::array<std::span<const glm::vec2>, 4> textureCoordinates;
	std::array<std::span<const glm::vec4>, 4> colors;
	std::span<const glm::vec4> boneWeights;
	std::span<const glm::uvec4> boneIndices;

	std::span<const uint32_t> indices;

	std::string_view name;
	std::string_view materialName;
	std::optional<Sphere> boundingSphere;
	std::optional<AABB> boundingBox;
};

struct WriteModelAssetArgs
{
	std::string_view vertexFormatName;
	std::span<const WriteModelAssetMesh> meshes;
	ModelAccessFlags accessFlags;
	std::span<const Animation> animations;
	const Skeleton* skeleton;
};

struct WriteModelAssetResult
{
	bool successful;
	std::string error;

	void AssertOk() const;
};

[[nodiscard]] EG_API WriteModelAssetResult WriteModelAsset(std::ostream& stream, const WriteModelAssetArgs& args);
} // namespace eg
