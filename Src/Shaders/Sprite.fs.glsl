#version 450 core

layout(set=0, binding=2) uniform sampler uSampler;
layout(set=1, binding=0) uniform texture2D uTexture;

layout(location=0) in vec2 vTexCoord;
layout(location=1) in vec4 vColor;

layout(location=0) out vec4 color_out;

layout(set=0, binding=1) uniform FlagsUB
{
	uint flags;
};

const uint RED_TO_ALPHA_BIT = 1 << 2;

const uint BLEND_ALPHA = 0;
const uint BLEND_ADDITIVE = 1;
const uint BLEND_OVERWRITE = 2;

void main()
{
	color_out = texture(sampler2D(uTexture, uSampler), vTexCoord);
	if ((flags & RED_TO_ALPHA_BIT) != 0)
		color_out = vec4(1, 1, 1, color_out.r);

	color_out *= vColor;

	uint blendMode = flags & 3;

	if (blendMode == BLEND_ALPHA)
		color_out.rgb *= color_out.a;
	else if (blendMode == BLEND_ADDITIVE)
		color_out.a = 0;
	else if (blendMode == BLEND_OVERWRITE)
		color_out.a = 1;
}
