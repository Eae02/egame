#include "ModelVertexFormat.hpp"
#include "../Assert.hpp"
#include "StdVertex.hpp"

#include <unordered_map>

namespace eg
{
static const ModelVertexAttribute PositionOnlyAttributes[] = {
	ModelVertexAttribute{ .type = ModelVertexAttributeType::Position_F32 },
};

static const ModelVertexAttribute StdVertexAosAttributes[] = {
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Position_F32,
		.offset = offsetof(StdVertex, position),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::TexCoord_F32,
		.offset = offsetof(StdVertex, texCoord),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Normal_I8,
		.offset = offsetof(StdVertex, normal),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Tangent_I8,
		.offset = offsetof(StdVertex, tangent),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Color_U8,
		.offset = offsetof(StdVertex, color),
	},
};

static const ModelVertexAttribute StdVertexAnim8AosAttributes[] = {
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Position_F32,
		.offset = offsetof(StdVertexAnim8, position),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::TexCoord_F32,
		.offset = offsetof(StdVertexAnim8, texCoord),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Normal_I8,
		.offset = offsetof(StdVertexAnim8, normal),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Tangent_I8,
		.offset = offsetof(StdVertexAnim8, tangent),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Color_U8,
		.offset = offsetof(StdVertexAnim8, color),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::BoneWeights_U8,
		.offset = offsetof(StdVertexAnim8, boneWeights),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::BoneIndices_U8,
		.offset = offsetof(StdVertexAnim8, boneIndices),
	},
};

static const ModelVertexAttribute StdVertexAnim16AosAttributes[] = {
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Position_F32,
		.offset = offsetof(StdVertexAnim16, position),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::TexCoord_F32,
		.offset = offsetof(StdVertexAnim16, texCoord),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Normal_I8,
		.offset = offsetof(StdVertexAnim16, normal),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Tangent_I8,
		.offset = offsetof(StdVertexAnim16, tangent),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::Color_U8,
		.offset = offsetof(StdVertexAnim16, color),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::BoneWeights_U8,
		.offset = offsetof(StdVertexAnim16, boneWeights),
	},
	ModelVertexAttribute{
		.type = ModelVertexAttributeType::BoneIndices_U8,
		.offset = offsetof(StdVertexAnim16, boneIndices),
	},
};

static const uint32_t PositionOnlyStreamsBytePerVertex[] = { sizeof(float) * 3 };
static const uint32_t StdVertexAosStreamsBytePerVertex[] = { sizeof(StdVertex) };
static const uint32_t StdVertexAnim8AosStreamsBytePerVertex[] = { sizeof(StdVertexAnim8) };
static const uint32_t StdVertexAnim16AosStreamsBytePerVertex[] = { sizeof(StdVertexAnim16) };

const ModelVertexFormat ModelVertexFormat::PositionOnly = {
	.attributes = PositionOnlyAttributes,
	.streamsBytesPerVertex = PositionOnlyStreamsBytePerVertex,
};

const ModelVertexFormat ModelVertexFormat::StdVertexAos = {
	.attributes = StdVertexAosAttributes,
	.streamsBytesPerVertex = StdVertexAosStreamsBytePerVertex,
};

const ModelVertexFormat ModelVertexFormat::StdVertexAnim8Aos = {
	.attributes = StdVertexAnim8AosAttributes,
	.streamsBytesPerVertex = StdVertexAnim8AosStreamsBytePerVertex,
};

const ModelVertexFormat ModelVertexFormat::StdVertexAnim16Aos = {
	.attributes = StdVertexAnim16AosAttributes,
	.streamsBytesPerVertex = StdVertexAnim16AosStreamsBytePerVertex,
};

static std::unordered_map<std::string_view, ModelVertexFormat> modelVertexFormats = {
	{ StdVertex::Name, ModelVertexFormat::StdVertexAos },
	{ StdVertexAnim8::Name, ModelVertexFormat::StdVertexAos },
	{ StdVertexAnim16::Name, ModelVertexFormat::StdVertexAos },
	{ "eg::PositionOnly", ModelVertexFormat::PositionOnly },
};

std::optional<ModelVertexAttribute> ModelVertexFormat::FindAttribute(
	ModelVertexAttributeType type, uint32_t typeIndex) const
{
	for (const ModelVertexAttribute& attrib : attributes)
		if (attrib.type == type && attrib.typeIndex == typeIndex)
			return attrib;
	return std::nullopt;
}

size_t ModelVertexFormat::CalculateBytesPerVertex() const
{
	size_t bytesPerVertex = 0;
	for (uint32_t streamBytesPerVertex : streamsBytesPerVertex)
		bytesPerVertex += streamBytesPerVertex;
	return bytesPerVertex;
}

size_t ModelVertexFormat::Hash() const
{
	size_t h = attributes.size() | (streamsBytesPerVertex.size() << 32);
	for (const ModelVertexAttribute& attrib : attributes)
		HashAppend(h, attrib.Hash());
	for (uint32_t bytesPerVertex : streamsBytesPerVertex)
		HashAppend(h, bytesPerVertex);
	return h;
}

size_t ModelVertexAttribute::Hash() const
{
	size_t hash = 0;
	HashAppend(hash, type);
	HashAppend(hash, typeIndex);
	HashAppend(hash, streamIndex);
	HashAppend(hash, offset);
	return hash;
}

void ModelVertexFormat::RegisterFormat(std::string_view name, ModelVertexFormat format)
{
	modelVertexFormats.emplace(name, format);
}

const ModelVertexFormat* ModelVertexFormat::FindFormatByName(std::string_view name)
{
	auto it = modelVertexFormats.find(name);
	return it == modelVertexFormats.end() ? nullptr : &it->second;
}

std::optional<std::string_view> ModelVertexFormat::FindNameByFormat(const ModelVertexFormat& format)
{
	uint64_t formatHash = format.Hash();
	for (auto [name, f] : modelVertexFormats)
	{
		if (formatHash == f.Hash())
			return name;
	}
	return std::nullopt;
}

size_t GetVertexAttributeByteWidth(ModelVertexAttributeType attributeType)
{
	switch (attributeType)
	{
	case ModelVertexAttributeType::Position_F32: return 3 * sizeof(float);
	case ModelVertexAttributeType::TexCoord_F32: return 2 * sizeof(float);
	case ModelVertexAttributeType::TexCoord_U16: return 2 * sizeof(uint16_t);
	case ModelVertexAttributeType::TexCoord_U8: return 2 * sizeof(uint8_t);
	case ModelVertexAttributeType::Normal_F32: return 3 * sizeof(float);
	case ModelVertexAttributeType::Normal_I10: return 4;
	case ModelVertexAttributeType::Normal_I8: return 4;
	case ModelVertexAttributeType::Tangent_F32: return 3 * sizeof(float);
	case ModelVertexAttributeType::Tangent_I10: return 4;
	case ModelVertexAttributeType::Tangent_I8: return 4;
	case ModelVertexAttributeType::Color_F32: return 4 * sizeof(float);
	case ModelVertexAttributeType::Color_U8: return 4 * sizeof(uint8_t);
	case ModelVertexAttributeType::BoneWeights_F32: return 4 * sizeof(float);
	case ModelVertexAttributeType::BoneWeights_U16: return 4 * sizeof(uint16_t);
	case ModelVertexAttributeType::BoneWeights_U8: return 4 * sizeof(uint8_t);
	case ModelVertexAttributeType::BoneIndices_U16: return 4 * sizeof(uint16_t);
	case ModelVertexAttributeType::BoneIndices_U8: return 4 * sizeof(uint8_t);
	}
	EG_UNREACHABLE
}
} // namespace eg
