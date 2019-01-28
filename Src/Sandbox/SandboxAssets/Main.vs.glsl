#version 450 core

#include <EGame.glh>

vec2 positions[] = vec2[] (vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.0, 0.5));
vec4 colors[] = vec4[] (vec4(1, 0, 0, 1), vec4(0, 1, 0, 1), vec4(0, 0, 1, 1));

layout(location=0) out vec4 color_out;

layout(push_constant) uniform PC
{
	mat2 uTransform;
};

void main()
{
	color_out = colors[gl_VertexIndex];
	gl_Position = vec4(uTransform * positions[gl_VertexIndex], 0.0, 1.0);
}
