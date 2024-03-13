#include "ParticleEmitterType.hpp"
#include "../../Assert.hpp"
#include "../../Assets/AssetLoad.hpp"
#include "../../IOUtils.hpp"
#include "../../Log.hpp"

namespace eg
{
static_assert(sizeof(SerializedParticleEmitter) == 4 * 21);

const AssetFormat ParticleEmitterType::AssetFormat{ "EG::ParticleEmitter", 0 };

inline Vec3Generator ReadVec3Generator(uint32_t type, MemoryReader& reader)
{
	Vec3Generator generator = [&]() -> Vec3Generator
	{
		switch (type)
		{
		case SphereVec3Generator::TYPE: return SphereVec3Generator();
		default: EG_PANIC("Unknown vec3 generator " << type)
		}
	}();

	std::visit([&](auto& gen) { gen.Read(reader); }, generator);

	return generator;
}

bool ParticleEmitterType::AssetLoader(const AssetLoadContext& loadContext)
{
	MemoryReader reader(loadContext.Data());

	const SerializedParticleEmitter sEmitter = reader.Read<SerializedParticleEmitter>();

	ParticleEmitterType& emitter = loadContext.CreateResult<ParticleEmitterType>();
	emitter.emissionRate = sEmitter.emissionRate;
	emitter.lifeTime = std::uniform_real_distribution<float>(sEmitter.lifeTimeMin, sEmitter.lifeTimeMax);
	emitter.initialRotation =
		std::uniform_real_distribution<float>(sEmitter.initialRotationMin, sEmitter.initialRotationMax);
	emitter.angularVelocity =
		std::uniform_real_distribution<float>(sEmitter.angularVelocityMin, sEmitter.angularVelocityMax);
	emitter.initialOpacity =
		std::uniform_real_distribution<float>(sEmitter.initialOpacityMin, sEmitter.initialOpacityMax);
	emitter.finalOpacity = std::uniform_real_distribution<float>(sEmitter.finalOpacityMin, sEmitter.finalOpacityMax);
	emitter.initialSize = std::uniform_real_distribution<float>(sEmitter.initialSizeMin, sEmitter.initialSizeMax);
	emitter.finalSize = std::uniform_real_distribution<float>(sEmitter.finalSizeMin, sEmitter.finalSizeMax);
	emitter.flags = sEmitter.flags;
	emitter.drag = sEmitter.drag;
	emitter.gravity = sEmitter.gravity;

	emitter.positionGenerator = ReadVec3Generator(sEmitter.positionGeneratorType, reader);
	emitter.velocityGenerator = ReadVec3Generator(sEmitter.velocityGeneratorType, reader);

	emitter.textureVariants.resize(sEmitter.numTextureVariants);
	reader.ReadToSpan<TextureVariant>(emitter.textureVariants);

	return true;
}
} // namespace eg
