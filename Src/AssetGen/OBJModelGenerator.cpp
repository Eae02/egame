#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Assets/ModelAsset.hpp"
#include "../EGame/Graphics/NormalTangentGen.hpp"
#include "../EGame/Log.hpp"

#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eg::asset_gen
{
struct VertexPtr
{
	int64_t position = -1;
	int64_t normal = -1;
	int64_t texCoord = -1;

	bool operator<(const VertexPtr& other) const
	{
		return std::tie(position, normal, texCoord) < std::tie(other.position, other.normal, other.texCoord);
	}

	bool operator==(const VertexPtr& other) const
	{
		return position == other.position && normal == other.normal && texCoord == other.texCoord;
	}
};

class OBJModelGenerator : public AssetGenerator
{
public:
	bool Generate(AssetGenerateContext& generateContext) override
	{
		std::string relSourcePath = generateContext.RelSourcePath();
		std::string sourcePath = generateContext.FileDependency(relSourcePath);
		std::ifstream sourceStream(sourcePath, std::ios::binary);
		if (!sourceStream)
		{
			Log(LogLevel::Error, "as", "Error opening asset file for reading: '{0}'", sourcePath);
			return false;
		}

		std::vector<std::string_view> parts;

		std::vector<glm::vec3> positions;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> texCoords;
		std::vector<std::array<VertexPtr, 3>> faces;

		struct OBJObject
		{
			std::string name;
			std::string material;
			size_t facesBegin;
			size_t facesEnd;
		};
		std::vector<OBJObject> objects;

		std::string currentMaterial;
		std::string currentObjectName;
		auto CommitCurrentObject = [&]()
		{
			size_t facesBegin = !objects.empty() ? objects.back().facesEnd : 0;
			if (facesBegin == faces.size())
				return;
			OBJObject& object = objects.emplace_back();
			object.material = currentMaterial;
			object.name = currentObjectName;
			object.facesBegin = facesBegin;
			object.facesEnd = faces.size();

			int nameSuffix = 2;
			while (true)
			{
				size_t numSameName = std::count_if(
					objects.begin(), objects.end(), [&](const OBJObject& o) { return o.name == object.name; });
				if (numSameName <= 1)
					break;
				object.name = currentObjectName + std::to_string(nameSuffix);
				nameSuffix++;
			}
		};

		bool addDefaultNormal = false;
		bool addDefaultTexCoord = false;

		std::string accessStr = generateContext.YAMLNode()["access"].as<std::string>("");
		ModelAccessFlags access = ParseModelAccessFlagsMode(accessStr, ModelAccessFlags::GPU);
		bool removeNameSuffix = generateContext.YAMLNode()["removeNameSuffix"].as<bool>(false);

		std::string line;
		while (!sourceStream.eof())
		{
			std::getline(sourceStream, line);
			std::string_view lineV = TrimString(line);
			if (lineV.empty() || lineV[0] == '#')
				continue;
			if (lineV.back() == '\r')
				lineV.remove_suffix(1);

			parts.clear();
			SplitString(lineV, ' ', parts);
			if (parts.empty())
				continue;

			auto ParseF = [&](std::string_view part) { return strtof(part.data(), nullptr); };

			if (parts[0] == "v")
			{
				if (parts.size() != 4)
				{
					Log(LogLevel::Error, "as", "Malformatted OBJ: {0}", sourcePath);
					return false;
				}
				positions.emplace_back(ParseF(parts[1]), ParseF(parts[2]), ParseF(parts[3]));
			}
			else if (parts[0] == "vn")
			{
				if (parts.size() != 4)
				{
					Log(LogLevel::Error, "as", "Malformatted OBJ: {0}", sourcePath);
					return false;
				}
				glm::vec3& normal = normals.emplace_back(ParseF(parts[1]), ParseF(parts[2]), ParseF(parts[3]));
				normal = glm::normalize(normal);
			}
			else if (parts[0] == "vt")
			{
				if (parts.size() != 3)
				{
					Log(LogLevel::Error, "as", "Malformatted OBJ: {0}", sourcePath);
					return false;
				}
				texCoords.emplace_back(ParseF(parts[1]), 1.0f - ParseF(parts[2]));
			}
			else if (parts[0] == "f")
			{
				if (parts.size() != 4)
				{
					Log(LogLevel::Error, "as", "OBJ file not triangulated: {0}", sourcePath);
					return false;
				}

				auto& face = faces.emplace_back();

				auto RemapRef = [&](int64_t r, int64_t max) { return r >= 0 ? (r - 1) : (max + r); };

				for (int i = 0; i < 3; i++)
				{
					std::string_view refStr = parts[i + 1];
					face[i].position = RemapRef(strtoll(refStr.data(), nullptr, 10), positions.size());

					size_t slash1 = refStr.find('/');
					size_t slash2 = refStr.rfind('/');
					if (slash1 != std::string_view::npos && (slash2 == slash1 || slash2 > slash1 + 1))
					{
						face[i].texCoord = RemapRef(strtoll(&refStr[slash1 + 1], nullptr, 10), texCoords.size());
					}
					else
					{
						addDefaultTexCoord = true;
					}

					if (slash1 != std::string_view::npos && slash2 != slash1)
					{
						face[i].normal = RemapRef(strtoll(&refStr[slash2 + 1], nullptr, 10), normals.size());
					}
					else
					{
						addDefaultNormal = true;
					}
				}
			}
			else if (parts[0] == "usemtl")
			{
				CommitCurrentObject();
				currentMaterial = TrimString(parts[1]);
			}
			else if (parts[0] == "o")
			{
				CommitCurrentObject();
				currentObjectName = TrimString(parts[1]);

				if (removeNameSuffix)
				{
					size_t finalUnderscore = currentObjectName.rfind('_');
					if (finalUnderscore != std::string::npos && finalUnderscore != 0)
					{
						currentObjectName = currentObjectName.substr(0, finalUnderscore);
					}
				}
			}
		}

		CommitCurrentObject();

		std::string vertexFormatName = generateContext.YAMLNode()["vertexFormat"].as<std::string>("");
		if (vertexFormatName.empty())
			vertexFormatName = "eg::StdVertexAos";

		if (addDefaultNormal)
		{
			normals.emplace_back(0, 1, 0);
		}
		if (addDefaultTexCoord)
		{
			texCoords.emplace_back(0, 0);
		}

		const bool flipWinding = !generateContext.YAMLNode()["flipWinding"].as<bool>(false);

		std::vector<WriteModelAssetMesh> meshes;
		std::map<VertexPtr, uint32_t> indexMap;
		std::vector<VertexPtr> verticesP;
		std::vector<std::unique_ptr<glm::vec3[], FreeDel>> tangentAllocations;

		LinearAllocator allocator;

		for (const OBJObject& object : objects)
		{
			indexMap.clear();
			verticesP.clear();

			size_t numIndices = (object.facesEnd - object.facesBegin) * 3;
			std::span<uint32_t> meshIndices = allocator.AllocateSpan<uint32_t>(numIndices);
			size_t nextIndexI = 0;

			// Remaps OBJ style vertex references to vertices and indices
			for (size_t f = object.facesBegin; f < object.facesEnd; f++)
			{
				for (VertexPtr faceVertexPtr : faces[f])
				{
					if (faceVertexPtr.normal == -1)
						faceVertexPtr.normal = normals.size() - 1;
					if (faceVertexPtr.texCoord == -1)
						faceVertexPtr.texCoord = texCoords.size() - 1;

					auto it = indexMap.find(faceVertexPtr);
					if (it == indexMap.end())
					{
						indexMap.emplace(faceVertexPtr, verticesP.size());
						meshIndices[nextIndexI++] = UnsignedNarrow<uint32_t>(verticesP.size());
						verticesP.push_back(faceVertexPtr);
					}
					else
					{
						meshIndices[nextIndexI++] = it->second;
					}
				}
			}

			// Potentially flips winding order
			if (flipWinding)
			{
				for (size_t i = 0; i < numIndices; i += 3)
				{
					std::swap(meshIndices[i], meshIndices[i + 2]);
				}
			}

			std::span<glm::vec3> meshPositions = allocator.AllocateSpan<glm::vec3>(verticesP.size());
			std::span<glm::vec2> meshTexCoords = allocator.AllocateSpan<glm::vec2>(verticesP.size());
			std::span<glm::vec3> meshNormals = allocator.AllocateSpan<glm::vec3>(verticesP.size());

			// Initializes AoS vertex data
			for (size_t i = 0; i < verticesP.size(); i++)
			{
				meshPositions[i] = positions[verticesP[i].position];
				meshTexCoords[i] = texCoords[verticesP[i].texCoord];
				meshNormals[i] = normals[verticesP[i].normal];
			}

			// Generates tangents for the vertices
			std::unique_ptr<glm::vec3[], FreeDel> tangents = GenerateTangents<uint32_t>(
				meshIndices, meshPositions.size(), [&](size_t i) { return meshPositions[i]; },
				[&](size_t i) { return meshTexCoords[i]; }, [&](size_t i) { return meshNormals[i]; });

			WriteModelAssetMesh mesh = {
				.positions = meshPositions,
				.normals = meshNormals,
				.tangents = std::span<const glm::vec3>(tangents.get(), verticesP.size()),
				.indices = meshIndices,
				.name = object.name,
				.materialName = object.material,
			};

			mesh.textureCoordinates[0] = meshTexCoords;

			meshes.push_back(mesh);

			tangentAllocations.push_back(std::move(tangents));
		}

		const WriteModelAssetArgs writeArgs = {
			.vertexFormatName = vertexFormatName,
			.meshes = meshes,
			.accessFlags = access,
		};

		WriteModelAssetResult result = WriteModelAsset(generateContext.outputStream, writeArgs);
		result.AssertOk();

		return result.successful;
	}
};

void RegisterOBJModelGenerator()
{
	RegisterAssetGenerator<OBJModelGenerator>("OBJModel", ModelAssetFormat);
}
} // namespace eg::asset_gen
