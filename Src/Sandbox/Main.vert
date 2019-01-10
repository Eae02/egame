#version 450 core

vec2 positions[] = vec2[] (vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(0.0, 0.5));
vec4 colors[] = vec4[] (vec4(1, 0, 0, 1), vec4(0, 1, 0, 1), vec4(0, 0, 1, 1));

layout(location=0) out vec4 color_out;

void main()
{
	color_out = colors[gl_VertexID];
	gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
}
