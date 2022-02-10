#include "../../EGame/Assets/ModelAsset.hpp"
#include "../../EGame/Assets/AssetGenerator.hpp"
#include "../../EGame/Graphics/StdVertex.hpp"
#include "../../EGame/Graphics/TangentGen.hpp"
#include "../../EGame/Log.hpp"
#include "../../EGame/IOUtils.hpp"
#include "../../EGame/Compression.hpp"
#include "../../EGame/Geometry/Sphere.hpp"
#include "../../EGame/Platform/FileSystem.hpp"

#include "GLTFAnimation.hpp"
#include "GLTFData.hpp"

#include <fstream>
#include <span>
#include <variant>
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
		int64_t meshIndex;
		int64_t skinIndex;
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
			mesh.meshIndex = meshIt->get<int64_t>();
			mesh.skinIndex = skinIt == nodeEl.end() ? -1 : skinIt->get<int64_t>();
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
		std::vector<StdVertexAnim16> vertices;
		
		bool flipWinding = false;
		bool hasSkeleton = false;
		bool hasTextureCoordinates = false;
		
		std::string name;
		int sourceNodeIndex;
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
	
	ImportedMesh ImportMesh(const GLTFData& gltfData, std::string name, const json& primitivesEl, const glm::mat4& transform)
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
		const Accessor* colorAccessor    = TryGetAccessor("COLOR_0", ElementType::VEC4);
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
		
		mesh.vertices.resize(numVertices);
		std::memset(mesh.vertices.data(), 0, sizeof(StdVertexAnim16) * numVertices);
		
		//Reads vertices
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
			
			if (colorAccessor)
			{
				const char* colorBufferPtr = colorBuffer + v * colorAccessor->byteStride;
				for (int c = 0; c < 4; c++)
				{
					mesh.vertices[v].color[c] = ReadFNormalized(colorBufferPtr, colorAccessor->componentType, c) * 255;
				}
			}
			
			if (weightsAccessor)
			{
				const char* weightsBufferPtr = weightsBuffer + v * weightsAccessor->byteStride;
				
				std::array<float, 4> weights;
				for (int c = 0; c < 4; c++)
				{
					weights[c] = ReadFNormalized(weightsBufferPtr, weightsAccessor->componentType, c);
				}
				
				SetBoneWeights(weights.data(), mesh.vertices[v].boneWeights);
			}
			
			if (jointsAccessor)
			{
				const void* jointsBufferPtr = jointsBuffer + v * jointsAccessor->byteStride;
				if (jointsAccessor->componentType == ComponentType::UInt8)
				{
					std::copy_n(static_cast<const uint8_t*>(jointsBufferPtr), 4, mesh.vertices[v].boneIndices);
				}
				else if (jointsAccessor->componentType == ComponentType::UInt16)
				{
					std::copy_n(static_cast<const uint16_t*>(jointsBufferPtr), 4, mesh.vertices[v].boneIndices);
				}
			}
		}
		
		mesh.boundingSphere = eg::Sphere::CreateEnclosing(std::span<const glm::vec3>(points.get(), numVertices));
		mesh.boundingBox = eg::AABB::CreateEnclosing(std::span<const glm::vec3>(points.get(), numVertices));
		
		return mesh;
	}
	
	enum class VertexType
	{
		Std,
		Anim8,
		Anim16
	};
	
	std::span<const StdVertex> ConvertVertices(
		const std::vector<StdVertexAnim16>& vertices, std::vector<StdVertex>& outputVector)
	{
		if (outputVector.size() < vertices.size())
			outputVector.resize(vertices.size());
		for (size_t v = 0; v < vertices.size(); v++)
		{
			std::memcpy(&outputVector[v], &vertices[v], sizeof(StdVertex));
		}
		return { outputVector.data(), vertices.size() };
	}
	
	std::span<const StdVertexAnim8> ConvertVertices(
		const std::vector<StdVertexAnim16>& vertices, std::vector<StdVertexAnim8>& outputVector)
	{
		if (outputVector.size() < vertices.size())
			outputVector.resize(vertices.size());
		for (size_t v = 0; v < vertices.size(); v++)
		{
			std::memcpy(&outputVector[v], &vertices[v], sizeof(StdVertex));
			std::copy_n(vertices[v].boneWeights, 4, outputVector[v].boneWeights);
			std::copy_n(vertices[v].boneIndices, 4, outputVector[v].boneIndices);
		}
		return { outputVector.data(), vertices.size() };
	}
	
	std::span<const StdVertexAnim16> ConvertVertices(
		const std::vector<StdVertexAnim16>& vertices, std::vector<StdVertexAnim16>& outputVector)
	{
		return vertices;
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
			
			const float scale = generateContext.YAMLNode()["scale"].as<float>(1.0f);
			const float sphereScale = generateContext.YAMLNode()["sphereScale"].as<float>(1.0f);
			const bool globalFlipWinding = !generateContext.YAMLNode()["flipWinding"].as<bool>(false);
			
			std::string vertexTypeString = generateContext.YAMLNode()["vertexType"].as<std::string>("std");
			VertexType vertexType = VertexType::Std;
			if (vertexTypeString == "anim8")
				vertexType = VertexType::Anim8;
			else if (vertexTypeString == "anim16")
				vertexType = VertexType::Anim16;
			else if (vertexTypeString != "std")
			{
				Log(LogLevel::Warning, "as", "Unknown mesh vertex type: '{0}'. "
					"Should be 'std', 'anim8' or 'anim16'.", vertexTypeString);
			}
			
			const std::string accessStr = generateContext.YAMLNode()["access"].as<std::string>("");
			const MeshAccess access = ParseMeshAccessMode(accessStr);
			
			json jsonRoot;
			GLTFData data;
			
			constexpr uint32_t GLBMagic = 0x46546C67;
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
				
				if (uri.starts_with(base64Prefix))
				{
					bufferData = Base64Decode(std::string_view(&uri[base64Prefix.size()]));
				}
				else
				{
					std::string depPath = generateContext.FileDependency(std::string(ParentPath(relSourcePath)) + uri);
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
				
				const auto& view = data.GetBufferView(accessorEl.at("bufferView").get<int64_t>());
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
			
			int64_t skinIndexToImport = -1;
			
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
						Log(LogLevel::Warning, "gltf", "{0}: Mesh '{1}' doesn't have texture coordinates.",
						    generateContext.AssetName(), name);
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
			
			// ** Imports the skeleton **
			ImportedSkeleton skeleton;
			if (skinIndexToImport != -1)
			{
				skeleton = ImportSkeleton(data, nodesArray, jsonRoot.at("skins").at(skinIndexToImport));
				if (vertexType == VertexType::Std)
				{
					Log(LogLevel::Warning, "gltf", "{0}: The model has a skeleton, "
					    "but vertex type (std) does not include bone indices.", generateContext.AssetName());
				}
				else if (vertexType == VertexType::Anim8 && skeleton.skeleton.NumBones() > 256)
				{
					Log(LogLevel::Warning, "gltf", "{0}: Vertex type anim8 was selected, but the skeleton has "
					    "more than 256 bones ({1}).", generateContext.AssetName(), skeleton.skeleton.NumBones());
				}
				
				for (ImportedMesh& mesh : meshes)
				{
					for (const StdVertexAnim16& vtx : mesh.vertices)
					{
						for (uint16_t boneId : vtx.boneIndices)
						{
							if (boneId != 0 && boneId >= skeleton.skeleton.NumBones())
							{
								Log(LogLevel::Error, "gltf", "{0}: Invalid vertex to bone reference, "
									"bone {1} does not exist.", generateContext.AssetName(), boneId);
								return false;
							}
							if (vertexType == VertexType::Anim8 && boneId >= 256)
							{
								Log(LogLevel::Error, "gltf", "{0}: Invalid vertex to bone reference, anim8 vertex "
									"format cannot reference bone {1}.", generateContext.AssetName(), boneId);
								return false;
							}
						}
					}
				}
			}
			
			// ** Imports animations **
			auto animationsIt = jsonRoot.find("animations");
			std::vector<Animation> animations;
			if (animationsIt != jsonRoot.end())
			{
				const size_t numTargets = skeleton.skeleton.NumBones() + meshes.size();
				const std::function<std::vector<int>(int)> getTargetIndicesFromNodeIndex = [&] (int nodeIndex)
				{
					std::vector<int> targetIndices;
					
					for (size_t i = 0; i < skeleton.boneIdNodeIndex.size(); i++)
					{
						if (skeleton.boneIdNodeIndex[i] == nodeIndex)
						{
							targetIndices.push_back(i);
						}
					}
					
					for (size_t i = 0; i < meshes.size(); i++)
					{
						if (meshes[i].sourceNodeIndex == nodeIndex)
						{
							targetIndices.push_back(skeleton.skeleton.NumBones() + i);
						}
					}
					
					return targetIndices;
				};
				
				animations.reserve(animationsIt->size());
				for (const json& animationEl : *animationsIt)
				{
					animations.push_back(ImportAnimation(data, animationEl, numTargets, getTargetIndicesFromNodeIndex));
				}
				
				std::sort(animations.begin(), animations.end(), AnimationNameCompare());
			}
			
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
								idx += meshes[dst].vertices.size();
							
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
			
			auto WriteMeshes = [&] <typename VertexType> (VertexType* vertices)
			{
				ModelAssetWriter<VertexType> writer(generateContext.outputStream);
				std::vector<VertexType> verticesOutput;
				for (ImportedMesh& mesh : meshes)
				{
					writer.WriteMesh(ConvertVertices(mesh.vertices, verticesOutput), mesh.indices, mesh.name, access,
					                 mesh.boundingSphere, mesh.boundingBox, materialNames[mesh.materialIndex]);
				}
				writer.End(skeleton.skeleton, animations);
			};
			
			switch (vertexType)
			{
			case VertexType::Anim16:
				WriteMeshes((StdVertexAnim16*)nullptr);
				break;
			case VertexType::Anim8:
				WriteMeshes((StdVertexAnim8*)nullptr);
				break;
			case VertexType::Std:
				WriteMeshes((StdVertex*)nullptr);
				break;
			}
			
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
