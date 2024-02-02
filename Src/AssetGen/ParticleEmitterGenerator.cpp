#include "../EGame/Assets/AssetGenerator.hpp"
#include "../EGame/Graphics/Particles/ParticleEmitterType.hpp"
#include "../EGame/IOUtils.hpp"
#include "../EGame/Log.hpp"

#include <charconv>
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace eg::asset_gen
{
inline void ParseMinMax(const YAML::Node& node, float& min, float& max, float def)
{
	if (node.IsSequence() && node.size() == 1)
	{
		min = max = node[0].as<float>(def);
	}
	else if (node.IsSequence() && node.size() > 1)
	{
		min = node[0].as<float>(def);
		max = node[1].as<float>(def);
	}
	else
	{
		min = max = node.as<float>(def);
	}
}

inline glm::vec3 ParseVec3(const YAML::Node& node)
{
	if (node.IsScalar())
		return glm::vec3(node.as<float>());
	if (node.IsSequence() && node.size() == 3)
		return glm::vec3(node[0].as<float>(), node[1].as<float>(), node[2].as<float>());
	eg::Log(LogLevel::Error, "as", "Invalid yaml vec3");
	return glm::vec3();
}

inline Vec3Generator ParseVec3Generator(const YAML::Node& node)
{
	if (node.IsNull() || !node.IsDefined())
		return SphereVec3Generator();
	if (node.IsSequence() && node.size() == 3)
		return SphereVec3Generator(Sphere(ParseVec3(node), 0));

	std::string shape = node["shape"].as<std::string>("");
	if (shape == "sphere")
	{
		return SphereVec3Generator(Sphere(ParseVec3(node["offset"]), node["radius"].as<float>(1.0f)));
	}

	eg::Log(LogLevel::Error, "as", "Unknown Vec3 generator shape '{0}'. Should be 'sphere'.", shape);
	return SphereVec3Generator();
}

class ParticleEmitterGenerator : public AssetGenerator
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

		YAML::Node rootYaml = YAML::Load(sourceStream);

		SerializedParticleEmitter emitter;

		emitter.emissionRate = rootYaml["emissionRate"].as<float>();
		ParseMinMax(rootYaml["lifeTime"], emitter.lifeTimeMin, emitter.lifeTimeMax, 1.0f);
		ParseMinMax(rootYaml["rotation"], emitter.initialRotationMin, emitter.initialRotationMax, 0.0f);
		ParseMinMax(rootYaml["angularVelocity"], emitter.angularVelocityMin, emitter.angularVelocityMax, 0.0f);
		ParseMinMax(rootYaml["opacity"], emitter.initialOpacityMin, emitter.initialOpacityMax, 1.0f);
		ParseMinMax(rootYaml["endOpacity"], emitter.finalOpacityMin, emitter.finalOpacityMax, 1.0f);
		ParseMinMax(rootYaml["size"], emitter.initialSizeMin, emitter.initialSizeMax, 1.0f);
		ParseMinMax(rootYaml["endSize"], emitter.finalSizeMin, emitter.finalSizeMax, 1.0f);

		emitter.initialRotationMin = glm::radians(emitter.initialRotationMin);
		emitter.initialRotationMax = glm::radians(emitter.initialRotationMax);
		emitter.angularVelocityMin = glm::radians(emitter.angularVelocityMin);
		emitter.angularVelocityMax = glm::radians(emitter.angularVelocityMax);

		emitter.flags = {};

		std::string blendMode = rootYaml["blend"].as<std::string>("alpha");
		if (blendMode == "additive")
		{
			emitter.flags |= ParticleFlags::BlendAdditive;
		}
		else if (blendMode != "alpha")
		{
			eg::Log(
				eg::LogLevel::Warning, "as",
				"Unknown particle blend mode: {0}. "
				"Should be 'alpha' or 'additive'.",
				blendMode);
		}

		emitter.gravity = rootYaml["gravity"].as<float>(0);
		emitter.drag = rootYaml["drag"].as<float>(0);

		Vec3Generator positionGenerator = ParseVec3Generator(rootYaml["position"]);
		Vec3Generator velocityGenerator = ParseVec3Generator(rootYaml["velocity"]);

		emitter.positionGeneratorType = std::visit([&](const auto& gen) { return gen.TYPE; }, positionGenerator);
		emitter.velocityGeneratorType = std::visit([&](const auto& gen) { return gen.TYPE; }, velocityGenerator);

		std::vector<ParticleEmitterType::TextureVariant> textureVariants;
		for (const YAML::Node& textureNode : rootYaml["textures"])
		{
			auto& textureVariant = textureVariants.emplace_back();
			textureVariant.x = textureNode["x"].as<int>(0);
			textureVariant.y = textureNode["y"].as<int>(0);
			textureVariant.width = textureNode["width"].as<int>();
			textureVariant.height = textureNode["height"].as<int>();
			textureVariant.numFrames = textureNode["frames"].as<int>(1);
		}

		if (textureVariants.empty())
		{
			eg::Log(LogLevel::Error, "as", "Empty textures array in particle emitter.");
			return false;
		}

		emitter.numTextureVariants = UnsignedNarrow<uint16_t>(textureVariants.size());

		generateContext.outputStream.write(reinterpret_cast<const char*>(&emitter), sizeof(SerializedParticleEmitter));

		std::visit([&](const auto& gen) { return gen.Write(generateContext.outputStream); }, positionGenerator);
		std::visit([&](const auto& gen) { return gen.Write(generateContext.outputStream); }, velocityGenerator);

		for (const ParticleEmitterType::TextureVariant& textureVariant : textureVariants)
		{
			BinWrite<int32_t>(generateContext.outputStream, textureVariant.x);
			BinWrite<int32_t>(generateContext.outputStream, textureVariant.y);
			BinWrite<int32_t>(generateContext.outputStream, textureVariant.width);
			BinWrite<int32_t>(generateContext.outputStream, textureVariant.height);
			BinWrite<int32_t>(generateContext.outputStream, textureVariant.numFrames);
		}

		return true;
	}
};

void RegisterParticleEmitterGenerator()
{
	RegisterAssetGenerator<ParticleEmitterGenerator>("ParticleEmitter", ParticleEmitterType::AssetFormat);
}
} // namespace eg::asset_gen
