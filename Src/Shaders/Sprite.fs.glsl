#version 450 core

layout(binding=0) uniform sampler2D uTexture;

layout(location=0) in vec2 vTexCoord;
layout(location=1) in vec4 vColor;

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	layout(offset=64) int redToAlpha;
};

void main()
{
	color_out = vColor;
	if (redToAlpha == 1)
	{
		color_out.a *= texture(uTexture, vTexCoord).r;
	}
	else
	{
		color_out *= texture(uTexture, vTexCoord);
	}
}
