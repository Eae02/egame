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
			const uint32_t numIndices = BinRead<uint32_t>(stream);
			if (numVertices == 0 || numIndices == 0)
				break;
			
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
			
			auto [vertices, indices] = modelBuilder.AddMesh(numVertices, numIndices, std::move(name), access, materialIndex, &sphere, &aabb);
			
			stream.read(static_cast<char*>(vertices), numVertices * vertexTypeIt->size);
			stream.read(static_cast<char*>(indices), numIndices * sizeof(uint32_t));
		}
		
		loadContext.CreateResult<Model>() = modelBuilder.CreateAndReset();
		return true;
	}
}
