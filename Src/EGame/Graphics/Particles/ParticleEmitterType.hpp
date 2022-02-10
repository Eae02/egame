#pragma once

#include <random>
#include <variant>
#include <vector>

#include "Vec3Generator.hpp"
#include "../../Utils.hpp"
#include "../../Assets/AssetFormat.hpp"

namespace eg
{
	enum class ParticleFlags : uint32_t
	{
		AlignToVelocity = 0x1,
		BlendAdditive = 0x2
	};
	
	EG_BIT_FIELD(ParticleFlags)
	
	struct EG_API ParticleEmitterType
	{
		struct TextureVariant
		{
			int x;
			int y;
			int width;
			int height;
			int numFrames;
		};
		
		float emissionRate;
		
		std::vector<TextureVariant> textureVariants;
		
		mutable std::uniform_real_distribution<float> lifeTime;
		
		Vec3Generator positionGenerator;
		Vec3Generator velocityGenerator;
		
		mutable std::uniform_real_distribution<float> initialRotation;
		mutable std::uniform_real_distribution<float> angularVelocity;
		
		mutable std::uniform_real_distribution<float> initialOpacity;
		mutable std::uniform_real_distribution<float> finalOpacity;
		
		mutable std::uniform_real_distribution<float> initialSize;
		mutable std::uniform_real_distribution<float> finalSize;
		
		float gravity;
		float drag;
		
		ParticleFlags flags;
		
		static const eg::AssetFormat AssetFormat;
		static bool AssetLoader(const class AssetLoadContext& loadContext);
	};
	
	struct SerializedParticleEmitter
	{
		float emissionRate;
		float lifeTimeMin;
		float lifeTimeMax;
		float initialRotationMax;
		float initialRotationMin;
		float angularVelocityMax;
		float angularVelocityMin;
		float initialOpacityMax;
		float initialOpacityMin;
		float finalOpacityMax;
		float finalOpacityMin;
		float initialSizeMax;
		float initialSizeMin;
		float finalSizeMax;
		float finalSizeMin;
		float gravity;
		float drag;
		ParticleFlags flags;
		uint32_t positionGeneratorType;
		uint32_t velocityGeneratorType;
		uint32_t numTextureVariants;
	};
}
