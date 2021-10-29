#include "ModelAsset.hpp"
#include "AssetLoad.hpp"

namespace eg
{
	const AssetFormat ModelAssetFormat { "EG::Model", 4 };
	
	std::vector<detail::ModelVertexType> detail::modelVertexTypes;
	
	bool ModelAssetLoader(const AssetLoadContext& loadContext)
	{
		MemoryStreambuf streamBuf(loadContext.Data());
		std::istream stream(&streamBuf);
		
		const uint32_t nameHash = BinRead<uint32_t>(stream);
		auto vertexTypeIt = std::find_if(detail::modelVertexTypes.begin(), detail::modelVertexTypes.end(),
			[&] (const detail::ModelVertexType& type) { return type.nameHash == nameHash; });
		
		if (vertexTypeIt == detail::modelVertexTypes.end())
		{
			Log(LogLevel::Error, "as", "Unknown model vertex type with hash {0}.", nameHash);
			return false;
		}
		
		ModelBuilderUnformatted modelBuilder(vertexTypeIt->type, vertexTypeIt->size,
			std::type_index(typeid(uint32_t)), sizeof(uint32_t), IndexType::UInt32);
		
		while (true)
		{
			const uint32_t numVertices = BinRead<uint32_t>(stream);
			if (numVertices == 0)
				break;
			
			const uint32_t numIndices = BinRead<uint32_t>(stream);
			
			MeshAccess access = (MeshAccess)BinRead<uint8_t>(stream);
			std::string materialName = BinReadString(stream);
			std::string name = BinReadString(stream);
			
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
			
			int materialIndex = modelBuilder.AddMaterial(materialName);
			
			auto [vertices, indices] = modelBuilder.AddMesh(
				numVertices, numIndices, std::move(name), access, materialIndex, &sphere, &aabb);
			
			stream.read(static_cast<char*>(vertices), numVertices * vertexTypeIt->size);
			stream.read(static_cast<char*>(indices), numIndices * sizeof(uint32_t));
		}
		
		Model& model = loadContext.CreateResult<Model>();
		model = modelBuilder.CreateAndReset();
		
		if (uint32_t numAnimationsPlus1 = BinRead<uint32_t>(stream))
		{
			model.skeleton = Skeleton::Deserialize(stream);
			
			const size_t numTargets = model.skeleton.NumBones() + model.NumMeshes();
			
			std::vector<Animation> animations;
			animations.reserve(numAnimationsPlus1 - 1);
			for (uint32_t i = 1; i < numAnimationsPlus1; i++)
			{
				animations.emplace_back(numTargets).Deserialize(stream);
			}
			
			model.SetAnimations(std::move(animations));
		}
		
		return true;
	}
	
	MeshAccess ParseMeshAccessMode(std::string_view accessModeString, MeshAccess def)
	{
		if (accessModeString == "gpu")
			return MeshAccess::GPUOnly;
		if (accessModeString == "cpu")
			return MeshAccess::CPUOnly;
		if (accessModeString == "all")
			return MeshAccess::All;
		if (accessModeString != "")
		{
			Log(LogLevel::Warning, "as", "Unknown mesh access mode: '{0}'. "
				"Should be 'gpu', 'cpu' or 'all'.", accessModeString);
		}
		return def;
	}
	
	void detail::ModelAssetWriterEnd(std::ostream& stream, const Skeleton& skeleton, std::span<const Animation> animations)
	{
		if (!std::is_sorted(animations.begin(), animations.end(), AnimationNameCompare()))
			EG_PANIC("Animations passed to ModelAssetWriter::End must be sorted.");
		
		BinWrite<uint32_t>(stream, 0);
		
		BinWrite(stream, static_cast<uint32_t>(animations.size() + 1));
		
		skeleton.Serialize(stream);
		
		for (const Animation& animation : animations)
		{
			animation.Serialize(stream);
		}
	}
}
