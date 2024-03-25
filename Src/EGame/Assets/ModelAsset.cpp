#include "ModelAsset.hpp"
#include "../Assert.hpp"
#include "AssetLoad.hpp"

namespace eg
{
const AssetFormat ModelAssetFormat{ "EG::Model", 5 };

bool ModelAssetLoader(const AssetLoadContext& loadContext)
{
	MemoryReader reader(loadContext.Data());

	const std::string_view vertexFormatName = reader.ReadString();
	const ModelVertexFormat* format = ModelVertexFormat::FindFormatByName(vertexFormatName);
	if (format == nullptr)
	{
		Log(LogLevel::Error, "as", "Unknown model vertex format: {0}.", vertexFormatName);
		return false;
	}

	const uint64_t vertexFormatHash = reader.Read<uint64_t>();
	if (vertexFormatHash != format->Hash())
	{
		Log(LogLevel::Error, "as", "Vertex format hash mismatch for format: {0}. Model may be out of date",
		    vertexFormatName);
		return false;
	}

	const uint32_t numVertexStreams = reader.Read<uint32_t>();
	if (numVertexStreams != format->streamsBytesPerVertex.size())
	{
		Log(LogLevel::Error, "as", "Vertex format stream count mismatch for format: {0}. Model may be out of date",
		    vertexFormatName);
		return false;
	}

	const uint32_t numMeshes = reader.Read<uint32_t>();
	const uint32_t numAnimations = reader.Read<uint32_t>();
	const ModelAccessFlags accessFlags = static_cast<ModelAccessFlags>(reader.Read<uint8_t>());

	std::vector<std::string> materialNames;

	uint32_t nextMeshFirstVertex = 0;
	uint32_t nextMeshFirstIndex = 0;

	std::vector<MeshDescriptor> meshes;
	meshes.reserve(numMeshes);
	for (uint32_t m = 0; m < numMeshes; m++)
	{
		const uint32_t numVertices = reader.Read<uint32_t>();
		const uint32_t numIndices = reader.Read<uint32_t>();
		const std::string_view materialName = reader.ReadString();
		const std::string_view meshName = reader.ReadString();

		Sphere sphere;
		sphere.position.x = reader.Read<float>();
		sphere.position.y = reader.Read<float>();
		sphere.position.z = reader.Read<float>();
		sphere.radius = reader.Read<float>();

		eg::AABB aabb;
		aabb.min.x = reader.Read<float>();
		aabb.min.y = reader.Read<float>();
		aabb.min.z = reader.Read<float>();
		aabb.max.x = reader.Read<float>();
		aabb.max.y = reader.Read<float>();
		aabb.max.z = reader.Read<float>();

		size_t materialIndex;
		auto materialIt = std::find(materialNames.begin(), materialNames.end(), materialName);
		if (materialIt != materialNames.end())
		{
			materialIndex = static_cast<size_t>(materialIt - materialNames.begin());
		}
		else
		{
			materialIndex = materialNames.size();
			materialNames.emplace_back(materialName);
		}

		meshes.push_back(MeshDescriptor{
			.name = std::string(meshName),
			.materialIndex = UnsignedNarrow<uint32_t>(materialIndex),
			.firstVertex = nextMeshFirstVertex,
			.firstIndex = nextMeshFirstIndex,
			.numVertices = numVertices,
			.numIndices = numIndices,
			.boundingSphere = sphere,
			.boundingAABB = aabb,
		});

		nextMeshFirstVertex += numVertices;
		nextMeshFirstIndex += numIndices;
	}

	const uint32_t totalIndices = nextMeshFirstIndex;
	const uint32_t totalVertices = nextMeshFirstVertex;

	const size_t totalIndexBytes = sizeof(uint32_t) * totalIndices;
	const size_t totalVertexBytes = format->CalculateBytesPerVertex() * static_cast<size_t>(totalVertices);

	std::span<const char> meshData = reader.ReadBytes(totalIndexBytes + totalVertexBytes);

	Skeleton skeleton = Skeleton::Deserialize(reader);

	const size_t numAnimationTargets = skeleton.NumBones() + numMeshes;
	std::vector<Animation> animations;
	animations.reserve(numAnimations);
	for (uint32_t i = 1; i < numAnimations; i++)
	{
		animations.emplace_back(numAnimationTargets).Deserialize(reader);
	}

	ModelCreateArgs modelCreateArgs = {
		.accessFlags = accessFlags,
		.meshes = std::move(meshes),
		.vertexData = meshData.subspan(totalIndexBytes),
		.numVertices = totalVertices,
		// indices might not be aligned for uint32/uint16, but that is probably not needed since the model constructor
		// just casts this buffer to char* and performs copies on it. But maybe this code should be changed to not use a
		// span of uint32/uint16
		.indices = std::span<const uint32_t>(reinterpret_cast<const uint32_t*>(meshData.data()), totalIndices),
		.vertexFormat = *format,
		.materialNames = std::move(materialNames),
		.animations = std::move(animations),
	};

	Model& model = loadContext.CreateResult<Model>(std::move(modelCreateArgs));
	model.skeleton = std::move(skeleton);

	return true;
}

ModelAccessFlags ParseModelAccessFlagsMode(std::string_view accessModeString, ModelAccessFlags def)
{
	if (accessModeString == "gpu")
		return ModelAccessFlags::GPU;
	if (accessModeString == "cpu")
		return ModelAccessFlags::CPU;
	if (accessModeString == "all")
		return ModelAccessFlags::GPU | ModelAccessFlags::CPU;
	if (accessModeString != "")
	{
		Log(LogLevel::Warning, "as",
		    "Unknown mesh access mode: '{0}'. "
		    "Should be 'gpu', 'cpu' or 'all'.",
		    accessModeString);
	}
	return def;
}

template <typename From, typename To>
To ConvertComponent(From value)
{
	using NumLimTo = std::numeric_limits<To>;
	using NumLimFrom = std::numeric_limits<From>;

	if constexpr (!NumLimFrom::is_integer && !NumLimTo::is_integer) // float to float
	{
		return static_cast<To>(value);
	}
	else if constexpr (NumLimFrom::is_integer && NumLimTo::is_integer) // int to int
	{
		EG_ASSERT(value <= std::numeric_limits<To>::max());
		return static_cast<To>(value);
	}
	else if constexpr (!NumLimFrom::is_integer && NumLimTo::is_integer && !NumLimTo::is_signed) // float to uint
	{
		constexpr int64_t Bounds = NumLimTo::max();
		float valueScaled = std::round(value * static_cast<float>(Bounds));
		return static_cast<To>(glm::clamp<int64_t>(static_cast<int64_t>(valueScaled), 0, Bounds));
	}
	else if constexpr (!NumLimFrom::is_integer && NumLimTo::is_integer && NumLimTo::is_signed) // float to sint
	{
		constexpr int64_t Bounds = NumLimTo::max();
		static_assert(-Bounds >= static_cast<int64_t>(NumLimTo::min()));
		float valueScaled = std::round(value * static_cast<float>(Bounds));
		return static_cast<To>(glm::clamp<int64_t>(static_cast<int64_t>(valueScaled), -Bounds, Bounds));
	}
	else
	{
		EG_PANIC("Invalid template arguments for ConvertComponent");
	}
}

template <typename OutCompT, typename InVecT>
void WriteVertexAttribute(
	std::span<char> output, size_t numVertices, uint32_t bytesPerVertex, std::span<const InVecT> input)
{
	if (input.empty())
		return;
	EG_ASSERT(input.size() == numVertices);

	for (size_t v = 0; v < input.size(); v++)
	{
		size_t dstOffset = v * bytesPerVertex;
		EG_ASSERT(dstOffset + sizeof(OutCompT) * InVecT::length() <= output.size());
		for (int c = 0; c < InVecT::length(); c++)
		{
			OutCompT value = ConvertComponent<typename InVecT::value_type, OutCompT>(input[v][c]);
			std::memcpy(output.data() + dstOffset + c * sizeof(OutCompT), &value, sizeof(OutCompT));
		}
	}
}

static std::pair<std::unique_ptr<char, FreeDel>, size_t> SerializeVertices(
	std::span<const WriteModelAssetMesh> meshes, const ModelVertexFormat& vertexFormat)
{
	const size_t numVertexStreams = vertexFormat.streamsBytesPerVertex.size();

	size_t totalVertices = 0;
	for (const WriteModelAssetMesh& mesh : meshes)
		totalVertices += mesh.positions.size();

	const size_t totalVertexBytes = vertexFormat.CalculateBytesPerVertex() * totalVertices;
	std::unique_ptr<char, FreeDel> vertexData(static_cast<char*>(std::calloc(1, totalVertexBytes)));

	std::vector<std::span<char>> vertexStreamDataSpans(numVertexStreams);
	size_t nextStreamOutputOffset = 0;
	for (size_t i = 0; i < numVertexStreams; i++)
	{
		size_t bytesForThisStream = vertexFormat.streamsBytesPerVertex[i] * totalVertices;
		vertexStreamDataSpans[i] = std::span<char>(vertexData.get() + nextStreamOutputOffset, bytesForThisStream);
		nextStreamOutputOffset += bytesForThisStream;
	}

	for (const ModelVertexAttribute& attrib : vertexFormat.attributes)
	{
		EG_ASSERT(attrib.streamIndex <= vertexFormat.streamsBytesPerVertex.size());

		const uint32_t bytesPerVertex = vertexFormat.streamsBytesPerVertex[attrib.streamIndex];
		size_t outputOffset = attrib.offset;

		for (const WriteModelAssetMesh& mesh : meshes)
		{
			const size_t meshNumVertices = mesh.positions.size();

			std::span<char> output =
				vertexStreamDataSpans[attrib.streamIndex].subspan(outputOffset, meshNumVertices * bytesPerVertex);

			outputOffset += output.size();

			switch (attrib.type)
			{
			case ModelVertexAttributeType::Position_F32:
				WriteVertexAttribute<float>(output, meshNumVertices, bytesPerVertex, mesh.positions);
				break;

			case ModelVertexAttributeType::TexCoord_F32:
				WriteVertexAttribute<float>(
					output, meshNumVertices, bytesPerVertex, mesh.textureCoordinates.at(attrib.typeIndex));
				break;
			case ModelVertexAttributeType::TexCoord_U16:
				WriteVertexAttribute<uint16_t>(
					output, meshNumVertices, bytesPerVertex, mesh.textureCoordinates.at(attrib.typeIndex));
				break;
			case ModelVertexAttributeType::TexCoord_U8:
				WriteVertexAttribute<uint8_t>(
					output, meshNumVertices, bytesPerVertex, mesh.textureCoordinates.at(attrib.typeIndex));
				break;

			case ModelVertexAttributeType::Normal_F32:
				WriteVertexAttribute<float>(output, meshNumVertices, bytesPerVertex, mesh.normals);
				break;
			case ModelVertexAttributeType::Normal_I10: EG_PANIC("unimplemented");
			case ModelVertexAttributeType::Normal_I8:
				WriteVertexAttribute<int8_t>(output, meshNumVertices, bytesPerVertex, mesh.normals);
				break;

			case ModelVertexAttributeType::Tangent_F32:
				WriteVertexAttribute<float>(output, meshNumVertices, bytesPerVertex, mesh.tangents);
				break;
			case ModelVertexAttributeType::Tangent_I10: EG_PANIC("unimplemented");
			case ModelVertexAttributeType::Tangent_I8:
				WriteVertexAttribute<int8_t>(output, meshNumVertices, bytesPerVertex, mesh.tangents);
				break;

			case ModelVertexAttributeType::Color_F32:
				WriteVertexAttribute<float>(output, meshNumVertices, bytesPerVertex, mesh.colors.at(attrib.typeIndex));
				break;
			case ModelVertexAttributeType::Color_U8:
				WriteVertexAttribute<uint8_t>(
					output, meshNumVertices, bytesPerVertex, mesh.colors.at(attrib.typeIndex));
				break;

			case ModelVertexAttributeType::BoneWeights_F32:
				WriteVertexAttribute<float>(output, meshNumVertices, bytesPerVertex, mesh.boneWeights);
				break;
			case ModelVertexAttributeType::BoneWeights_U16:
				WriteVertexAttribute<uint16_t>(output, meshNumVertices, bytesPerVertex, mesh.boneWeights);
				break;
			case ModelVertexAttributeType::BoneWeights_U8:
				WriteVertexAttribute<uint8_t>(output, meshNumVertices, bytesPerVertex, mesh.boneWeights);
				break;

			case ModelVertexAttributeType::BoneIndices_U16:
				WriteVertexAttribute<uint16_t>(output, meshNumVertices, bytesPerVertex, mesh.boneIndices);
				break;
			case ModelVertexAttributeType::BoneIndices_U8:
				WriteVertexAttribute<uint8_t>(output, meshNumVertices, bytesPerVertex, mesh.boneIndices);
				break;
			}
		}
	}

	return { std::move(vertexData), totalVertexBytes };
}

WriteModelAssetResult WriteModelAsset(MemoryWriter& writer, const WriteModelAssetArgs& args)
{
	if (!std::is_sorted(args.animations.begin(), args.animations.end(), AnimationNameCompare()))
		return WriteModelAssetResult{ .error = "animations not sorted by name" };

	const ModelVertexFormat* vertexFormat = ModelVertexFormat::FindFormatByName(args.vertexFormatName);
	if (vertexFormat == nullptr)
		return WriteModelAssetResult{ .error = "vertex format not found" };

	uint32_t numVertexStreams = UnsignedNarrow<uint32_t>(vertexFormat->streamsBytesPerVertex.size());

	writer.WriteString(args.vertexFormatName);
	writer.Write<uint64_t>(vertexFormat->Hash());
	writer.Write<uint32_t>(numVertexStreams);
	writer.Write<uint32_t>(UnsignedNarrow<uint32_t>(args.meshes.size()));
	writer.Write<uint32_t>(UnsignedNarrow<uint32_t>(args.animations.size()));
	writer.Write(static_cast<uint8_t>(args.accessFlags));

	for (const WriteModelAssetMesh& mesh : args.meshes)
	{
		writer.Write<uint32_t>(UnsignedNarrow<uint32_t>(mesh.positions.size()));
		writer.Write<uint32_t>(UnsignedNarrow<uint32_t>(mesh.indices.size()));
		writer.WriteString(mesh.materialName);
		writer.WriteString(mesh.name);

		Sphere boundingSphere;
		if (mesh.boundingSphere.has_value())
			boundingSphere = *mesh.boundingSphere;
		else
			boundingSphere = Sphere::CreateEnclosing(mesh.positions);

		AABB boundingBox;
		if (mesh.boundingBox.has_value())
			boundingBox = *mesh.boundingBox;
		else
			boundingBox = AABB::CreateEnclosing(mesh.positions);

		writer.Write(boundingSphere.position.x);
		writer.Write(boundingSphere.position.y);
		writer.Write(boundingSphere.position.z);
		writer.Write(boundingSphere.radius);
		writer.Write(boundingBox.min.x);
		writer.Write(boundingBox.min.y);
		writer.Write(boundingBox.min.z);
		writer.Write(boundingBox.max.x);
		writer.Write(boundingBox.max.y);
		writer.Write(boundingBox.max.z);
	}

	for (const WriteModelAssetMesh& mesh : args.meshes)
	{
		writer.WriteMultiple(mesh.indices);
	}

	auto [vertexData, vertexDataBytes] = SerializeVertices(args.meshes, *vertexFormat);
	writer.WriteBytes({ vertexData.get(), vertexDataBytes });

	static Skeleton emptySkeleton;
	(args.skeleton ? args.skeleton : &emptySkeleton)->Serialize(writer);

	for (const Animation& animation : args.animations)
		animation.Serialize(writer);

	return WriteModelAssetResult{ .successful = true };
}

void WriteModelAssetResult::AssertOk() const
{
	if (!successful)
	{
		detail::PanicImpl(error);
	}
}
} // namespace eg
