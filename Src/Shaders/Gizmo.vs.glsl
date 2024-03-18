#version 450 core

layout(location=0) in vec3 position_in;

layout(set=0, binding=0) uniform ParamsUB
{
	mat4 worldViewProj;
};

void main()
{
	gl_Position = worldViewProj * vec4(position_in, 1.0);
}
