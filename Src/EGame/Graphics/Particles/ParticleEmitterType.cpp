#include "ParticleEmitterType.hpp"
#include "../../Assets/AssetLoad.hpp"
#include "../../Log.hpp"
#include "../../IOUtils.hpp"

namespace eg
{
	const AssetFormat ParticleEmitterType::AssetFormat { "EG::ParticleEmitter", 0 };
	
	inline Vec3Generator ReadVec3Generator(uint32_t type, std::istream& stream)
	{
		Vec3Generator generator = [&] () -> Vec3Generator
		{
			switch (type)
			{
			case SphereVec3Generator::TYPE:
				return SphereVec3Generator();
			default:
				EG_PANIC("Unknown vec3 generator " << type)
			}
		}();
		
		std::visit([&] (auto& gen) { gen.Read(stream); }, generator);
		
		return generator;
	}
	
	bool ParticleEmitterType::AssetLoader(const AssetLoadContext& loadContext)
	{
		const auto* sEmitter = reinterpret_cast<const SerializedParticleEmitter*>(loadContext.Data().data());
		
		ParticleEmitterType& emitter = loadContext.CreateResult<ParticleEmitterType>();
		emitter.emissionRate = sEmitter->emissionRate;
		emitter.lifeTime = std::uniform_real_distribution<float>(sEmitter->lifeTimeMin, sEmitter->lifeTimeMax);
		emitter.initialRotation = std::uniform_real_distribution<float>(sEmitter->initialRotationMin, sEmitter->initialRotationMax);
		emitter.angularVelocity = std::uniform_real_distribution<float>(sEmitter->angularVelocityMin, sEmitter->angularVelocityMax);
		emitter.initialOpacity = std::uniform_real_distribution<float>(sEmitter->initialOpacityMin, sEmitter->initialOpacityMax);
		emitter.finalOpacity = std::uniform_real_distribution<float>(sEmitter->finalOpacityMin, sEmitter->finalOpacityMax);
		emitter.initialSize = std::uniform_real_distribution<float>(sEmitter->initialSizeMin, sEmitter->initialSizeMax);
		emitter.finalSize = std::uniform_real_distribution<float>(sEmitter->finalSizeMin, sEmitter->finalSizeMax);
		emitter.flags = sEmitter->flags;
		emitter.drag = sEmitter->drag;
		emitter.gravity = sEmitter->gravity;
		
		MemoryStreambuf streambuf(
			loadContext.Data().data() + sizeof(SerializedParticleEmitter),
			loadContext.Data().data() + loadContext.Data().SizeBytes());
		std::istream stream(&streambuf);
		
		emitter.positionGenerator = ReadVec3Generator(sEmitter->positionGeneratorType, stream);
		emitter.velocityGenerator = ReadVec3Generator(sEmitter->velocityGeneratorType, stream);
		
		emitter.textureVariants.resize(sEmitter->numTextureVariants);
		for (uint32_t i = 0; i < sEmitter->numTextureVariants; i++)
		{
			emitter.textureVariants[i].x = BinRead<int32_t>(stream);
			emitter.textureVariants[i].y = BinRead<int32_t>(stream);
			emitter.textureVariants[i].width = BinRead<int32_t>(stream);
			emitter.textureVariants[i].height = BinRead<int32_t>(stream);
			emitter.textureVariants[i].numFrames = BinRead<int32_t>(stream);
		}
		
		return true;
	}
}
