#include "ModelAsset.hpp"
#include "../Assert.hpp"
#include "AssetLoad.hpp"

namespace eg
{
const AssetFormat ModelAssetFormat{ "EG::Model", 5 };

bool ModelAssetLoader(const AssetLoadContext& loadContext)
{
	MemoryStreambuf streamBuf(loadContext.Data());
	std::istream stream(&streamBuf);

	const std::string vertexFormatName = BinReadString(stream);
	const ModelVertexFormat* format = ModelVertexFormat::FindFormatByName(vertexFormatName);
	if (format == nullptr)
	{
		Log(LogLevel::Error, "as", "Unknown model vertex format: {0}.", vertexFormatName);
		return false;
	}

	const uint64_t vertexFormatHash = BinRead<uint64_t>(stream);
	if (vertexFormatHash != format->Hash())
	{
		Log(LogLevel::Error, "as", "Vertex format hash mismatch for format: {0}. Model may be out of date",
		    vertexFormatName);
		return false;
	}

	const uint32_t numVertexStreams = BinRead<uint32_t>(stream);
	if (numVertexStreams != format->streamsBytesPerVertex.size())
	{
		Log(LogLevel::Error, "as", "Vertex format stream count mismatch for format: {0}. Model may be out of date",
		    vertexFormatName);
		return false;
	}

	const uint32_t numMeshes = BinRead<uint32_t>(stream);
	const uint32_t numAnimations = BinRead<uint32_t>(stream);
	const ModelAccessFlags accessFlags = static_cast<ModelAccessFlags>(BinRead<uint8_t>(stream));

	std::vector<std::string> materialNames;

	uint32_t nextMeshFirstVertex = 0;
	uint32_t nextMeshFirstIndex = 0;

	std::vector<MeshDescriptor> meshes;
	meshes.reserve(numMeshes);
	for (uint32_t m = 0; m < numMeshes; m++)
	{
		const uint32_t numVertices = BinRead<uint32_t>(stream);
		const uint32_t numIndices = BinRead<uint32_t>(stream);
		const std::string materialName = BinReadString(stream);
		const std::string meshName = BinReadString(stream);

		Sphere sphere;
		sphere.position.x = BinRead<float>(stream);
		sphere.position.y = BinRead<float>(stream);
		sphere.position.z = BinRead<float>(stream);
		sphere.radius = BinRead<float>(stream);

		eg::AABB aabb;
		aabb.min.x = BinRead<float>(stream);
		aabb.min.y = BinRead<float>(stream);
		aabb.min.z = BinRead<float>(stream);
		aabb.max.x = BinRead<float>(stream);
		aabb.max.y = BinRead<float>(stream);
		aabb.max.z = BinRead<float>(stream);

		uint32_t materialIndex;
		auto materialIt = std::find(materialNames.begin(), materialNames.end(), materialName);
		if (materialIt != materialNames.end())
		{
			materialIndex = materialIt - materialNames.begin();
		}
		else
		{
			materialIndex = materialNames.size();
			materialNames.push_back(std::move(materialName));
		}

		meshes.push_back(MeshDescriptor{
			.name = std::move(meshName),
			.materialIndex = materialIndex,
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

	std::unique_ptr<char[]> meshData = std::make_unique<char[]>(totalIndexBytes + totalVertexBytes);
	stream.read(meshData.get(), totalIndexBytes + totalVertexBytes);

	Skeleton skeleton = Skeleton::Deserialize(stream);

	const size_t numAnimationTargets = skeleton.NumBones() + numMeshes;
	std::vector<Animation> animations;
	animations.reserve(numAnimations);
	for (uint32_t i = 1; i < numAnimations; i++)
	{
		animations.emplace_back(numAnimationTargets).Deserialize(stream);
	}

	ModelCreateArgs modelCreateArgs = {
		.accessFlags = accessFlags,
		.meshes = std::move(meshes),
		.vertexData = std::span<const char>(meshData.get() + totalIndexBytes, totalVertexBytes),
		.numVertices = totalVertices,
		.indices = std::span<const uint32_t>(reinterpret_cast<const uint32_t*>(meshData.get()), totalIndices),
		.vertexFormat = *format,
		.materialNames = std::move(materialNames),
		.animations = std::move(animations),
		.memoryForCpuAccess = std::move(meshData),
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
		for (size_t c = 0; c < InVecT::length(); c++)
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

	const size_t bytesPerVertex = vertexFormat.CalculateBytesPerVertex();

	const size_t totalVertexBytes = bytesPerVertex * totalVertices;
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

WriteModelAssetResult WriteModelAsset(std::ostream& stream, const WriteModelAssetArgs& args)
{
	if (!std::is_sorted(args.animations.begin(), args.animations.end(), AnimationNameCompare()))
		return WriteModelAssetResult{ .error = "animations not sorted by name" };

	const ModelVertexFormat* vertexFormat = ModelVertexFormat::FindFormatByName(args.vertexFormatName);
	if (vertexFormat == nullptr)
		return WriteModelAssetResult{ .error = "vertex format not found" };

	uint32_t numVertexStreams = UnsignedNarrow<uint32_t>(vertexFormat->streamsBytesPerVertex.size());

	BinWriteString(stream, args.vertexFormatName);
	BinWrite<uint64_t>(stream, vertexFormat->Hash());
	BinWrite<uint32_t>(stream, numVertexStreams);
	BinWrite<uint32_t>(stream, UnsignedNarrow<uint32_t>(args.meshes.size()));
	BinWrite<uint32_t>(stream, UnsignedNarrow<uint32_t>(args.animations.size()));
	BinWrite(stream, static_cast<uint8_t>(args.accessFlags));

	for (const WriteModelAssetMesh& mesh : args.meshes)
	{
		BinWrite<uint32_t>(stream, UnsignedNarrow<uint32_t>(mesh.positions.size()));
		BinWrite<uint32_t>(stream, UnsignedNarrow<uint32_t>(mesh.indices.size()));
		BinWriteString(stream, mesh.materialName);
		BinWriteString(stream, mesh.name);

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

		BinWrite<float>(stream, boundingSphere.position.x);
		BinWrite<float>(stream, boundingSphere.position.y);
		BinWrite<float>(stream, boundingSphere.position.z);
		BinWrite<float>(stream, boundingSphere.radius);
		BinWrite<float>(stream, boundingBox.min.x);
		BinWrite<float>(stream, boundingBox.min.y);
		BinWrite<float>(stream, boundingBox.min.z);
		BinWrite<float>(stream, boundingBox.max.x);
		BinWrite<float>(stream, boundingBox.max.y);
		BinWrite<float>(stream, boundingBox.max.z);
	}

	for (const WriteModelAssetMesh& mesh : args.meshes)
	{
		stream.write(reinterpret_cast<const char*>(mesh.indices.data()), mesh.indices.size_bytes());
	}

	auto [vertexData, vertexDataBytes] = SerializeVertices(args.meshes, *vertexFormat);
	stream.write(vertexData.get(), vertexDataBytes);

	static Skeleton emptySkeleton;
	(args.skeleton ? args.skeleton : &emptySkeleton)->Serialize(stream);

	for (const Animation& animation : args.animations)
		animation.Serialize(stream);

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
