#version 450 core

layout(binding=0) uniform sampler2D uTexture;

layout(location=0) in vec2 vTexCoord;
layout(location=1) in vec4 vColor;

layout(location=0) out vec4 color_out;

void main()
{
	color_out = vColor * texture(uTexture, vTexCoord);
}
