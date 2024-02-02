#include "../EGame/API.hpp"

namespace eg::asset_gen
{
void RegisterShaderGenerator();
void RegisterTexture2DGenerator();
void RegisterTexture2DArrayGenerator();
void RegisterOBJModelGenerator();
void RegisterGLTFModelGenerator();
void RegisterParticleEmitterGenerator();
void RegisterFontGenerator();
void RegisterOGGVorbisGenerator();
} // namespace eg::asset_gen

EG_C_EXPORT void Init()
{
	eg::asset_gen::RegisterShaderGenerator();
	eg::asset_gen::RegisterTexture2DGenerator();
	eg::asset_gen::RegisterTexture2DArrayGenerator();
	eg::asset_gen::RegisterOBJModelGenerator();
	eg::asset_gen::RegisterGLTFModelGenerator();
	eg::asset_gen::RegisterParticleEmitterGenerator();
	eg::asset_gen::RegisterFontGenerator();
	eg::asset_gen::RegisterOGGVorbisGenerator();
}
