#version 450 core

layout(location=0) out vec4 color_out;

layout(location=0) in vec4 vColor;
layout(location=1) in vec2 vTexCoord;

layout(set = 0, binding = 1) uniform sampler uSampler;
layout(set = 1, binding = 0) uniform texture2D uTexture;

void main()
{
	color_out = vColor * texture(sampler2D(uTexture, uSampler), vTexCoord);
}
