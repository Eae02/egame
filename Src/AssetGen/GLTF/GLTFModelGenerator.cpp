#include "../../EGame/Assets/ModelAsset.hpp"
#include "../../EGame/Assets/AssetGenerator.hpp"
#include "../../EGame/Graphics/StdVertex.hpp"
#include "../../EGame/Graphics/TangentGen.hpp"
#include "../../EGame/Log.hpp"
#include "../../EGame/IOUtils.hpp"
#include "../../EGame/Sphere.hpp"
#include "GLTFData.hpp"

#include <fstream>
#include <span>
#include <nlohmann/json.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace nlohmann;

template <typename IsValidTp>
inline std::string AddNameSuffix(const std::string& originalName, IsValidTp isValid)
{
	std::string finalName = originalName;
	std::ostringstream nameStream;
	int suffix = 1;
	while (!isValid(finalName))
	{
		suffix++;
		
		nameStream.str("");
		nameStream << originalName << "_" << suffix;
		finalName = nameStream.str();
	}
	
	return finalName;
}

namespace eg::asset_gen::gltf
{
	static glm::mat4 ParseNodeTransform(const nlohmann::json& node)
	{
		auto matrixIt = node.find("matrix");
		if (matrixIt != node.end())
		{
			glm::mat4 transform;
			
			for (int c = 0; c < 4; c++)
			{
				for (int r = 0; r < 4; r++)
				{
					transform[c][r] = (*matrixIt)[c * 4 + r];
				}
			}
			
			return transform;
		}
		
		glm::vec3 scale(1.0f);
		glm::quat rotation;
		glm::vec3 translation;
		
		auto scaleIt = node.find("scale");
		if (scaleIt != node.end())
		{
			scale = glm::vec3((*scaleIt)[0], (*scaleIt)[1], (*scaleIt)[2]);
		}
		
		auto rotationIt = node.find("rotation");
		if (rotationIt != node.end())
		{
			rotation = glm::quat((*rotationIt)[3], (*rotationIt)[0], (*rotationIt)[1], (*rotationIt)[2]);
		}
		
		auto translationIt = node.find("translation");
		if (translationIt != node.end())
		{
			translation = glm::vec3((*translationIt)[0], (*translationIt)[1], (*translationIt)[2]);
		}
		
		return glm::translate(glm::mat4(1), translation) * glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
	}
	
	struct MeshToImport
	{
		long meshIndex;
		long skinIndex;
		size_t nodeIndex;
		std::string name;
		glm::mat4 transform;
	};
	
	//Recursive function for walking through the node tree collecting meshes to be imported.
	static void WalkNodeTree(const nlohmann::json& nodesArray, size_t nodeIndex, std::vector<MeshToImport>& meshes,
	                         const glm::mat4& transform)
	{
		const nlohmann::json& nodeEl = nodesArray[nodeIndex];
		
		auto meshIt = nodeEl.find("mesh");
		
		glm::mat4 nodeTransform = transform * ParseNodeTransform(nodeEl);
		
		if (meshIt != nodeEl.end())
		{
			std::string name;
			
			auto nameIt = nodeEl.find("name");
			if (nameIt != nodeEl.end())
			{
				name = nameIt->get<std::string>();
			}
			
			auto skinIt = nodeEl.find("skin");
			
			MeshToImport mesh;
			mesh.meshIndex = meshIt->get<long>();
			mesh.skinIndex = skinIt == nodeEl.end() ? -1 : skinIt->get<long>();
			mesh.nodeIndex = nodeIndex;
			mesh.name = std::move(name);
			mesh.transform = nodeTransform;
			
			meshes.push_back(std::move(mesh));
		}
		
		auto childrenIt = nodeEl.find("children");
		if (childrenIt != nodeEl.end())
		{
			for (size_t i = 0; i < childrenIt->size(); i++)
			{
				WalkNodeTree(nodesArray, (*childrenIt)[i].get<size_t>(), meshes, nodeTransform);
			}
		}
	}
	
	inline json LoadGLB(std::istream& stream, GLTFData& data)
	{
		const uint32_t ChunkTypeJSON = 0x4E4F534A;
		const uint32_t ChunkTypeBinary = 0x004E4942;
		
		uint32_t fileVersion = BinRead<uint32_t>(stream);
		if (fileVersion != 2)
			throw std::runtime_error("Unsupported GLB version");
		
		uint32_t fileLength = BinRead<uint32_t>(stream);
		
		std::vector<char> jsonChunkData;
		std::vector<char> binaryChunkData;
		
		//Parses chunks
		while (stream.tellg() < fileLength)
		{
			uint32_t chunkLength = BinRead<uint32_t>(stream);
			uint32_t chunkType = BinRead<uint32_t>(stream);
			
			switch (chunkType)
			{
			case ChunkTypeJSON:
				jsonChunkData.resize(chunkLength);
				stream.read(jsonChunkData.data(), chunkLength);
				break;
			
			case ChunkTypeBinary:
				binaryChunkData.resize(chunkLength);
				stream.read(binaryChunkData.data(), chunkLength);
				break;
			
			default:
				stream.seekg(chunkLength, std::ios_base::cur);
				break;
			}
		}
		
		if (jsonChunkData.empty())
		{
			throw std::runtime_error("No JSON chunk.");
		}
		
		json jsonRoot = json::parse(jsonChunkData);
		
		const nlohmann::json& buffersArray = jsonRoot.at("buffers");
		if (buffersArray.size() >= 1 && !binaryChunkData.empty() &&
		    buffersArray[0].find("uri") == buffersArray[0].end())
		{
			data.AddBuffer(std::move(binaryChunkData));
		}
		
		return jsonRoot;
	}
	
	struct ImportedMesh
	{
		std::vector<uint32_t> indices;
		std::vector<StdVertex> vertices;
		
		bool flipWinding = false;
		bool hasSkeleton = false;
		bool hasTextureCoordinates = false;
		
		std::string name;
		size_t sourceNodeIndex;
		uint16_t materialIndex;
		glm::mat4 transform;
		
		Sphere boundingSphere;
		AABB boundingBox;
	};
	
	template <typename T>
	inline void CopyIndices(const char* in, uint32_t* out, int stride, int count)
	{
		for (int i = 0; i < count; i++)
		{
			out[i] = *reinterpret_cast<const T*>(in + i * stride);
		}
	}
	
	inline ImportedMesh ImportMesh(const GLTFData& gltfData, std::string name, const json& primitivesEl,
	                               const glm::mat4& transform)
	{
		ImportedMesh mesh;
		
		if (glm::determinant(transform) < 0)
			mesh.flipWinding = !mesh.flipWinding;
		
		// ** Indices **
		
		//Gets the indices accessor and data pointer
		const Accessor& indicesAccessor = gltfData.GetAccessor(primitivesEl.at("indices"));
		const char* indicesData = gltfData.GetAccessorData(indicesAccessor);
		
		//Reads indices
		mesh.indices.resize(indicesAccessor.elementCount);
		switch (indicesAccessor.componentType)
		{
		case ComponentType::UInt8:
			CopyIndices<uint8_t>(indicesData, mesh.indices.data(), indicesAccessor.byteStride, indicesAccessor.elementCount);
			break;
		case ComponentType::UInt16:
			CopyIndices<uint16_t>(indicesData, mesh.indices.data(), indicesAccessor.byteStride, indicesAccessor.elementCount);
			break;
		case ComponentType::UInt32:
			CopyIndices<uint32_t>(indicesData, mesh.indices.data(), indicesAccessor.byteStride, indicesAccessor.elementCount);
			break;
		default:
			break;
		}
		
		// ** Vertices **
		
		const json& attributesEl = primitivesEl.at("attributes");
		
		//Fetches acccessors
		auto TryGetAccessor = [&] (const char* accessorName, ElementType elementType) -> const Accessor*
		{
			auto it = attributesEl.find(accessorName);
			if (it == attributesEl.end())
				return nullptr;
			
			const Accessor& accessor = gltfData.GetAccessor(*it);
			if (accessor.elementType != elementType)
				return nullptr;
			return &accessor;
		};
		
		const Accessor* positionAccessor = TryGetAccessor("POSITION", ElementType::VEC3);
		const Accessor* normalAccessor   = TryGetAccessor("NORMAL", ElementType::VEC3);
		const Accessor* texCoordAccessor = TryGetAccessor("TEXCOORD_0", ElementType::VEC2);
		const Accessor* colorAccessor = TryGetAccessor("COLOR_0", ElementType::VEC4);
		const Accessor* weightsAccessor  = TryGetAccessor("WEIGHTS_0", ElementType::VEC4);
		const Accessor* jointsAccessor   = TryGetAccessor("JOINTS_0", ElementType::VEC4);
		
		//Checks required accessors
		if (positionAccessor == nullptr || positionAccessor->componentType != ComponentType::Float)
			throw std::runtime_error("Invalid or missing position accessor.");
		if (normalAccessor == nullptr || normalAccessor->componentType != ComponentType::Float)
			throw std::runtime_error("Invalid or missing normal accessor.");
		
		mesh.hasTextureCoordinates = texCoordAccessor != nullptr;
		
		auto numVertices = static_cast<size_t>(positionAccessor->elementCount);
		
		const char* positionBuffer = gltfData.GetAccessorData(*positionAccessor);
		const char* normalBuffer   = gltfData.GetAccessorData(*normalAccessor);
		const char* texCoordBuffer = texCoordAccessor ? gltfData.GetAccessorData(*texCoordAccessor) : nullptr;
		const char* colorBuffer = colorAccessor ? gltfData.GetAccessorData(*colorAccessor) : nullptr;
		const char* weightsBuffer  = weightsAccessor ? gltfData.GetAccessorData(*weightsAccessor) : nullptr;
		const char* jointsBuffer   = jointsAccessor ? gltfData.GetAccessorData(*jointsAccessor) : nullptr;
		
		//Reads vertices
		mesh.vertices.resize(numVertices);
		auto points = std::make_unique<glm::vec3[]>(numVertices);
		for (size_t v = 0; v < numVertices; v++)
		{
			glm::vec3 pos = *reinterpret_cast<const glm::vec3*>(positionBuffer + v * positionAccessor->byteStride);
			glm::vec3 normal = *reinterpret_cast<const glm::vec3*>(normalBuffer + v * normalAccessor->byteStride);
			
			pos = glm::vec3(transform * glm::vec4(pos, 1.0f));
			normal = glm::normalize(glm::vec3(transform * glm::vec4(normal, 0.0f)));
			points[v] = pos;
			
			for (int i = 0; i < 3; i++)
			{
				mesh.vertices[v].position[i] = pos[i];
				mesh.vertices[v].normal[i] = FloatToSNorm(normal[i]);
			}
			
			if (texCoordAccessor)
			{
				const char* tcBufferPtr = texCoordBuffer + v * texCoordAccessor->byteStride;
				for (int c = 0; c < 2; c++)
				{
					mesh.vertices[v].texCoord[c] = ReadFNormalized(tcBufferPtr, texCoordAccessor->componentType, c);
				}
			}
			else
			{
				for (int c = 0; c < 2; c++)
				{
					mesh.vertices[v].texCoord[c] = 0;
				}
			}
			
			if (colorAccessor)
			{
				const char* colorBufferPtr = colorBuffer + v * colorAccessor->byteStride;
				for (int c = 0; c < 4; c++)
				{
					mesh.vertices[v].color[c] = ReadFNormalized(colorBufferPtr, colorAccessor->componentType, c) * 255;
				}
			}
			else
			{
				*reinterpret_cast<uint32_t*>(mesh.vertices[v].color) = 0;
			}
			
			/*
			if (weightsAccessor)
			{
				const char* weightsBufferPtr = weightsBuffer + v * weightsAccessor->byteStride;
				
				std::array<float, 4> weights;
				for (int c = 0; c < 4; c++)
				{
					weights[c] = ReadFNormalized(weightsBufferPtr, weightsAccessor->componentType, c);
				}
				
				mesh.vertices[v].SetBoneWeights(weights);
			}
			
			if (jointsAccessor)
			{
				const char* jointsBufferPtr = jointsBuffer + v * jointsAccessor->byteStride;
				
				std::array<uint8_t, 4> boneIds;
				auto ReadBoneIds = [&] (auto dataPtr) { std::copy_n(dataPtr, 4, boneIds.begin()); };
				
				if (jointsAccessor->componentType == ComponentType::UInt8)
					ReadBoneIds(reinterpret_cast<const uint8_t*>(jointsBufferPtr));
				else if (jointsAccessor->componentType == ComponentType::UInt16)
					ReadBoneIds(reinterpret_cast<const uint16_t*>(jointsBufferPtr));
				
				mesh.vertices[v].SetBoneIds(boneIds);
			}*/
		}
		
		mesh.boundingSphere = eg::Sphere::CreateEnclosing(std::span<const glm::vec3>(points.get(), numVertices));
		mesh.boundingBox = eg::AABB::CreateEnclosing(std::span<const glm::vec3>(points.get(), numVertices));
		
		return mesh;
	}
	
	class GLTFModelGenerator : public AssetGenerator
	{
	public:
		bool Generate(AssetGenerateContext& generateContext) override
		{
			std::string relSourcePath = generateContext.RelSourcePath();
			std::string sourcePath = generateContext.FileDependency(relSourcePath);
			std::ifstream stream(sourcePath, std::ios::binary);
			if (!stream)
			{
				Log(LogLevel::Error, "as", "Error opening asset file for reading: '{0}'", sourcePath);
				return false;
			}
			
			float scale = generateContext.YAMLNode()["scale"].as<float>(1.0f);
			float sphereScale = generateContext.YAMLNode()["sphereScale"].as<float>(1.0f);
			bool globalFlipWinding = !generateContext.YAMLNode()["flipWinding"].as<bool>(false);
			
			json jsonRoot;
			GLTFData data;
			
			const uint32_t GLBMagic = 0x46546C67;
			if (BinRead<uint32_t>(stream) == GLBMagic)
			{
				jsonRoot = LoadGLB(stream, data);
			}
			else
			{
				stream.seekg(0, std::ios::beg);
				jsonRoot = json::parse(stream);
			}
			
			static const std::string_view base64Prefix = "data:application/octet-stream;base64,";
			
			// ** Parses and reads buffers **
			const json& buffersArray = jsonRoot.at("buffers");
			for (const json& bufferEl : buffersArray)
			{
				auto uriIt = bufferEl.find("uri");
				if (uriIt == bufferEl.end())
					continue;
				
				std::string uri = uriIt->get<std::string>();
				std::vector<char> bufferData;
				
				if (StringStartsWith(uri, base64Prefix))
				{
					bufferData = Base64Decode(std::string_view(&uri[base64Prefix.size()]));
				}
				else
				{
					std::string depPath = generateContext.FileDependency(uri);
					std::ifstream depStream(depPath, std::ios::binary);
					if (!depStream)
					{
						Log(LogLevel::Error, "as", "Error opening GLTF buffer for reading: '{0}'", depPath);
						return false;
					}
					
					depStream.seekg(0, std::ios_base::end);
					std::streamoff size = depStream.tellg();
					depStream.seekg(0, std::ios_base::beg);
					
					bufferData.resize(size);
					depStream.read(bufferData.data(), size);
				}
				
				data.AddBuffer(std::move(bufferData));
			}
			
			// ** Parses buffer views **
			const json& bufferViewsArray = jsonRoot.at("bufferViews");
			for (const json& bufferViewEl : bufferViewsArray)
			{
				BufferView bufferView;
				bufferView.bufferIndex = bufferViewEl.at("buffer");
				bufferView.byteOffset = bufferViewEl.at("byteOffset");
				
				auto byteStrideIt = bufferViewEl.find("byteStride");
				if (byteStrideIt != bufferViewEl.end())
					bufferView.byteStride = *byteStrideIt;
				else
					bufferView.byteStride = 0;
				
				data.AddBufferView(bufferView);
			}
			
			// ** Parses accessors **
			for (const json& accessorEl : jsonRoot.at("accessors"))
			{
				Accessor accessor = { };
				
				const auto& view = data.GetBufferView(accessorEl.at("bufferView").get<long>());
				accessor.bufferIndex = view.bufferIndex;
				accessor.elementCount = accessorEl.at("count");
				accessor.componentType = static_cast<ComponentType>(accessorEl.at("componentType").get<int>());
				accessor.elementType = GLTFData::ParseElementType(accessorEl.at("type").get<std::string>());
				accessor.byteOffset = view.byteOffset;
				
				auto byteOffsetIt = accessorEl.find("byteOffset");
				if (byteOffsetIt != accessorEl.end())
					accessor.byteOffset += byteOffsetIt->get<int>();
				
				const int componentsPerElement = ComponentsPerElement(accessor.elementType);
				
				if (view.byteStride != 0)
				{
					accessor.byteStride = view.byteStride;
				}
				else
				{
					switch (accessor.componentType)
					{
					case ComponentType::UInt8:
						accessor.byteStride = 1 * componentsPerElement;
						break;
					case ComponentType::UInt16:
						accessor.byteStride = 2 * componentsPerElement;
						break;
					case ComponentType::UInt32:
					case ComponentType::Float:
						accessor.byteStride = 4 * componentsPerElement;
						break;
					}
				}
				
				data.AddAccessor(accessor);
			}
			
			// ** Imports materials **
			std::vector<std::string> materialNames;
			auto materialsIt = jsonRoot.find("materials");
			if (materialsIt != jsonRoot.end())
			{
				for (size_t i = 0; i < materialsIt->size(); i++)
				{
					std::string name = "Unnamed Material";
					
					auto nameIt = (*materialsIt)[i].find("name");
					if (nameIt != (*materialsIt)[i].end())
					{
						name = nameIt->get<std::string>();
					}
					
					name = AddNameSuffix(name, [&] (const std::string& cName)
					{
						return !std::any_of(materialNames.begin(), materialNames.end(),
							[&] (const std::string& n) { return cName == n; });
					});
					
					materialNames.push_back(std::move(name));
				}
			}
			
			//Keeps track of whether a material has been referenced by a mesh so unused materials can be removed later.
			std::vector<bool> materialsReferenced(materialNames.size());
			
			int defaultMaterialIndex = -1;
			
			// ** Prepares to import meshes **
			int sceneIndex = 0;
			auto sceneIt = jsonRoot.find("scene");
			if (sceneIt != jsonRoot.end())
				sceneIndex = sceneIt->get<uint32_t>();
			
			const json& sceneNodesArray = jsonRoot.at("scenes").at(sceneIndex).at("nodes");
			const json& nodesArray = jsonRoot.at("nodes");
			const json& meshesArray = jsonRoot.at("meshes");
			
			//Walks the node tree to find meshes to import
			std::vector<MeshToImport> meshesToImport;
			for (const nlohmann::json& sceneNode : sceneNodesArray)
			{
				glm::mat4 rootTransform = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
				WalkNodeTree(nodesArray, sceneNode.get<size_t>(), meshesToImport, rootTransform);
			}
			
			long skinIndexToImport = -1;
			
			// ** Imports meshes **
			std::vector<ImportedMesh> meshes;
			for (MeshToImport& meshToImport : meshesToImport)
			{
				const json& meshEl = meshesArray.at(meshToImport.meshIndex);
				
				std::string baseName = std::move(meshToImport.name);
				
				if (baseName.empty())
				{
					auto nameIt = meshEl.find("name");
					if (nameIt != meshEl.end())
						baseName = nameIt->get<std::string>();
				}
				
				bool hasSkeleton = false;
				if (meshToImport.skinIndex != -1)
				{
					//If there currently isn't any skin being imported, mark this one for import.
					if (skinIndexToImport == -1)
					{
						skinIndexToImport = meshToImport.skinIndex;
					}
					
					//This mesh can have a skeleton if it's skin index matches the one to be imported.
					if (skinIndexToImport == meshToImport.skinIndex)
					{
						hasSkeleton = true;
					}
					else
					{
						eg::Log(eg::LogLevel::Warning, "as", "Model has multiple skins but only one will be imported.");
					}
				}
				
				const json& primitivesArray = meshEl.at("primitives");
				for (size_t i = 0; i < primitivesArray.size(); i++)
				{
					std::string name = baseName;
					if (primitivesArray.size() > 1)
						name += "_" + std::to_string(i);
					
					name = AddNameSuffix(name, [&](const std::string& cName)
					{
						return !std::any_of(meshes.begin(), meshes.end(), [&](const ImportedMesh& mesh)
						{
							return mesh.name == cName;
						});
					});
					
					uint16_t materialIndex;
					
					const json& primitiveEl = primitivesArray.at(i);
					auto materialIt = primitiveEl.find("material");
					if (materialIt != primitiveEl.end())
					{
						materialIndex = *materialIt;
						materialsReferenced.at(materialIndex) = true;
					}
					else
					{
						//Creates a default material if one doesn't already exist
						if (defaultMaterialIndex == -1)
						{
							defaultMaterialIndex = static_cast<int>(materialNames.size());
							materialNames.push_back(AddNameSuffix("default", [&](const std::string& mName)
							{
								return !Contains(materialNames, mName);
							}));
						}
						materialIndex = static_cast<uint16_t>(defaultMaterialIndex);
					}
					
					ImportedMesh mesh = ImportMesh(data, std::move(name), primitiveEl, meshToImport.transform);
					mesh.boundingSphere.radius *= sphereScale;
					mesh.materialIndex = materialIndex;
					mesh.sourceNodeIndex = meshToImport.nodeIndex;
					mesh.hasSkeleton = hasSkeleton;
					mesh.flipWinding ^= globalFlipWinding;
					
					if (!mesh.hasTextureCoordinates)
					{
						std::ostringstream msgStream;
						msgStream << "Mesh '" << name << "' doesn't have texture coordinates!";
						eg::Log(eg::LogLevel::Warning, "as", "{0}", msgStream.str());
					}
					
					meshes.push_back(std::move(mesh));
				}
			}
			
			// ** Removes unused materials **
			for (size_t i = 0; i < materialsReferenced.size(); i++)
			{
				if (materialsReferenced[i])
					continue;
				
				materialNames.erase(materialNames.begin() + i);
				
				for (ImportedMesh& mesh : meshes)
				{
					if (mesh.materialIndex > i)
						mesh.materialIndex--;
				}
			}
			
			//Writes a log message for each of the imported materials.
			for (const std::string& materialName : materialNames)
			{
				std::ostringstream msgStream;
				msgStream << "Imported material '" << materialName << "'";
				eg::Log(eg::LogLevel::Info, "as", "{0}", msgStream.str());
			}
			
			//ImportedModel model(std::move(meshes), std::move(materialNames));
			
			/*
			// ** Imports the skeleton **
			if (skinIndexToImport != -1)
			{
				model.Skeleton().Import(data, nodesArray, jsonRoot.at("skins").at(skinIndexToImport));
			}
			
			// ** Imports animations **
			auto animationsIt = jsonRoot.find("animations");
			if (animationsIt != jsonRoot.end())
			{
				std::vector<Jade::Animation> animations;
				animations.reserve(animationsIt->size());
				
				for (const json& animationEl : *animationsIt)
				{
					Jade::Animation animation = ImportAnimation(data, animationEl, model);
					
					//Writes a log message for the animation
					std::ostringstream messageStream;
					messageStream << "Imported animation '" << animation.Name() << "'.";
					importArgs.WriteMessage(Jade::ResMessageType::Notification, messageStream.str());
					
					animations.push_back(std::move(animation));
				}
				
				std::sort(ALL(animations), [&] (const Jade::Animation& a, const Jade::Animation& b)
				{
					return a.Name() < b.Name();
				});
				
				model.SetAnimations(std::move(animations));
			}*/
			
			//Applies winding
			for (ImportedMesh& mesh : meshes)
			{
				if (mesh.flipWinding)
				{
					for (size_t i = 0; i < mesh.indices.size(); i += 3)
					{
						std::swap(mesh.indices[i], mesh.indices[i + 2]);
					}
					mesh.flipWinding = false;
				}
			}
			
			if (generateContext.YAMLNode()["mergeMeshes"].as<bool>(false))
			{
				for (size_t src = 1; src < meshes.size();)
				{
					bool merged = false;
					for (size_t dst = 0; dst < src; dst++)
					{
						if (meshes[src].materialIndex == meshes[dst].materialIndex)
						{
							for (uint32_t& idx : meshes[src].indices)
							{
								idx += meshes[dst].vertices.size();
							}
							
							meshes[dst].vertices.insert(meshes[dst].vertices.end(),
								meshes[src].vertices.begin(), meshes[src].vertices.end());
							
							meshes[dst].indices.insert(meshes[dst].indices.end(),
								meshes[src].indices.begin(), meshes[src].indices.end());
							
							merged = true;
							break;
						}
					}
					
					if (merged)
					{
						meshes.erase(meshes.begin() + src);
					}
					else
					{
						src++;
					}
				}
			}
			
			ModelAssetWriter<StdVertex> writer(generateContext.outputStream);
			
			for (ImportedMesh& mesh : meshes)
			{
				writer.WriteMesh(mesh.vertices, mesh.indices, mesh.name, eg::MeshAccess::GPUOnly,
				                 mesh.boundingSphere, mesh.boundingBox, materialNames[mesh.materialIndex]);
			}
			
			writer.End();
			
			return true;
		}
	};
}

namespace eg::asset_gen
{
	void RegisterGLTFModelGenerator()
	{
		RegisterAssetGenerator<gltf::GLTFModelGenerator>("GLTFModel", ModelAssetFormat);
	}
}
