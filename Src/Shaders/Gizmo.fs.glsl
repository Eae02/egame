#version 450 core

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	layout(offset=64) vec4 color;
};

void main()
{
	color_out = color;
}
