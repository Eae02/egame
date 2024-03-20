#version 450 core

layout(location=0) out vec4 color_out;

layout(set=0, binding=0) uniform ParamsUB
{
	mat4 worldViewProj;
	vec4 color;
};

void main()
{
	color_out = color;
}
