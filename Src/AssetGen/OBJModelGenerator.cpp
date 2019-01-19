#include "../../Inc/Common.hpp"
#include "../EGame/Assets/ModelAsset.hpp"
#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Graphics/StdVertex.hpp"
#include "../EGame/Graphics/TangentGen.hpp"
#include "../EGame/Log.hpp"
#include "../EGame/IOUtils.hpp"

#include <fstream>
#include <charconv>

#define PARSE_FLOAT(str, flt) \
if (std::from_chars(str.data(), str.data() + str.size(), flt).ec == std::errc::invalid_argument) \
	return false;

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
			
			int64_t defaultNormalIdx = -1;
			int64_t defaultTexCoordIdx = -1;
			
			std::string line;
			while (!sourceStream.eof())
			{
				std::getline(sourceStream, line);
				if (line[0] == '#')
					continue;
				
				parts.clear();
				SplitString(line, ' ', parts);
				if (parts.empty())
					continue;
				
				auto ParseF = [&] (std::string_view part)
				{
					return strtof(part.data(), nullptr);
				};
				
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
					texCoords.emplace_back(ParseF(parts[1]), ParseF(parts[2]));
				}
				else if (parts[0] == "f")
				{
					if (parts.size() != 4)
					{
						Log(LogLevel::Error, "as", "OBJ file not triangulated: {0}", sourcePath);
						return false;
					}
					
					auto& face = faces.emplace_back();
					
					auto RemapRef = [&] (int64_t r, int64_t max)
					{
						return r >= 0 ? (r - 1) : (max + r);
					};
					
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
							if (defaultTexCoordIdx == -1)
							{
								defaultTexCoordIdx = texCoords.size();
								texCoords.emplace_back(0, 0);
							}
							face[i].texCoord = defaultTexCoordIdx;
						}
						
						if (slash1 != std::string_view::npos && slash2 != slash1)
						{
							face[i].normal = RemapRef(strtoll(&refStr[slash2 + 1], nullptr, 10), normals.size());
						}
						else
						{
							if (defaultNormalIdx == -1)
							{
								defaultNormalIdx = normals.size();
								normals.emplace_back(0, 1, 0);
							}
							face[i].normal = defaultNormalIdx;
						}
					}
				}
			}
			
			//Remaps OBJ style vertex references to vertices and indices
			std::map<VertexPtr, uint32_t> indexMap;
			std::vector<VertexPtr> verticesP;
			std::vector<uint32_t> indices;
			for (const auto& face : faces)
			{
				for (const VertexPtr& faceVertexPtr : face)
				{
					auto it = indexMap.find(faceVertexPtr);
					if (it == indexMap.end())
					{
						indexMap.emplace(faceVertexPtr, verticesP.size());
						indices.push_back(verticesP.size());
						verticesP.push_back(faceVertexPtr);
					}
					else
					{
						indices.push_back(it->second);
					}
				}
			}
			
			//Initializes AoS vertex data
			std::vector<StdVertex> vertices(verticesP.size());
			for (size_t i = 0; i < vertices.size(); i++)
			{
				std::copy_n(&positions[verticesP[i].position].x, 3, vertices[i].position);
				std::copy_n(&texCoords[verticesP[i].texCoord].x, 2, vertices[i].texCoord);
				glm::vec3 n = normals[verticesP[i].normal];
				for (int j = 0; j < 3; j++)
					vertices[i].normal[j] = FloatToSNorm(n[j]);
			}
			
			//Generates tangents for the vertices
			GenerateTangents(vertices.size(), indices.size(),
				[&] (size_t i) { return positions[verticesP[i].position]; },
				[&] (size_t i) { return texCoords[verticesP[i].texCoord]; },
				[&] (size_t i) { return normals[verticesP[i].normal]; },
				[&] (size_t i) { return indices[i]; },
				[&] (size_t i, const glm::vec3& tangent)
				{
					for (int j = 0; j < 3; j++)
						vertices[i].tangent[j] = FloatToSNorm(tangent[j]);
				});
			
			ModelAssetWriter<StdVertex> writer(generateContext.outputStream);
			writer.WriteMesh(vertices, indices, "", MeshAccess::All);
			writer.End();
			
			return true;
		}
	};
	
	void RegisterOBJModelGenerator()
	{
		RegisterAssetGenerator<OBJModelGenerator>("OBJModel", ModelAssetFormat);
	}
}
