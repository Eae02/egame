#version 450 core

layout(set=0, binding=2) uniform sampler uSampler;
layout(set=1, binding=0) uniform texture2D uTexture;

layout(location=0) in vec2 vTexCoord;
layout(location=1) in vec4 vColor;

layout(location=0) out vec4 color_out;

layout(set=0, binding=1) uniform FlagsUB
{
	uint blendMode;
	uint effectMode;
};

const uint EFFECT_NONE = 0;
const uint EFFECT_RED_TO_ALPHA = 1;
const uint EFFECT_DISTANCE_FIELD = 2;

const uint BLEND_ALPHA = 0;
const uint BLEND_ADDITIVE = 1;
const uint BLEND_OVERWRITE = 2;

const float SDF_DIST_LO = 0.9;
const float SDF_DIST_HI = 1.0;

void main()
{
	color_out = texture(sampler2D(uTexture, uSampler), vTexCoord);
	
	if (effectMode == EFFECT_DISTANCE_FIELD)
		color_out = vec4(1, 1, 1, clamp((color_out.r - SDF_DIST_LO) / (SDF_DIST_HI - SDF_DIST_LO), 0.0, 1.0));
	else if (effectMode == EFFECT_RED_TO_ALPHA)
		color_out = vec4(1, 1, 1, color_out.r);

	color_out *= vColor;

	if (blendMode == BLEND_ALPHA)
		color_out.rgb *= color_out.a;
	else if (blendMode == BLEND_ADDITIVE)
		color_out.a = 0;
	else if (blendMode == BLEND_OVERWRITE)
		color_out.a = 1;
}
