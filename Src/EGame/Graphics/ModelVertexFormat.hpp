#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace eg
{
enum class ModelVertexAttributeType : uint8_t
{
	Position_F32,
	TexCoord_F32,
	TexCoord_U16,
	TexCoord_U8,
	Normal_F32,
	Normal_I10,
	Normal_I8,
	Tangent_F32,
	Tangent_I10,
	Tangent_I8,
	Color_F32,
	Color_U8,
	BoneWeights_F32,
	BoneWeights_U16,
	BoneWeights_U8,
	BoneIndices_U16,
	BoneIndices_U8,
};

size_t GetVertexAttributeByteWidth(ModelVertexAttributeType attributeType);

struct ModelVertexAttribute
{
	ModelVertexAttributeType type;
	uint32_t typeIndex;
	uint32_t offset;
	uint32_t streamIndex;

	size_t Hash() const;
};

static_assert(sizeof(ModelVertexAttribute) == 16);
static_assert(std::is_trivial_v<ModelVertexAttribute>);

struct ModelVertexFormat
{
	std::span<const ModelVertexAttribute> attributes;
	std::span<const uint32_t> streamsBytesPerVertex;

	static const ModelVertexFormat PositionOnly;
	static const ModelVertexFormat StdVertexAos;
	static const ModelVertexFormat StdVertexAnim8Aos;
	static const ModelVertexFormat StdVertexAnim16Aos;

	static void RegisterFormat(std::string_view name, ModelVertexFormat format);
	static const ModelVertexFormat* FindFormatByName(std::string_view name);
	static std::optional<std::string_view> FindNameByFormat(const ModelVertexFormat& format);

	std::optional<ModelVertexAttribute> FindAttribute(ModelVertexAttributeType type, uint32_t typeIndex) const;

	size_t CalculateBytesPerVertex() const;

	size_t Hash() const;
};
} // namespace eg
