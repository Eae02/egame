#version 450 core

layout(binding=0) uniform sampler2D uTexture;

layout(location=0) in vec2 vTexCoord;
layout(location=1) in vec4 vColor;

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	layout(offset=64) int flags;
};

vec4 swizzle(vec4 inp)
{
	return (flags == 1) ? vec4(1, 1, 1, inp.r) : inp;
}

void main()
{
	color_out = vColor * swizzle(texture(uTexture, vTexCoord));
}
