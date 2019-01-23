#version 450 core

layout(location=0) in vec2 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec4 color_in;

layout(location=0) out vec2 vTexCoord;
layout(location=1) out vec4 vColor;

layout(push_constant) uniform PC
{
	mat3 transform;
};

void main()
{
	vTexCoord = texCoord_in;
	vColor = color_in;
	
	vec2 sPos = (transform * vec3(position_in, 1.0)).xy;
	
	gl_Position = vec4(sPos, 0.0, 1.0);
}
