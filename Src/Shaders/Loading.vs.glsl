#version 450 core

layout(location=0) out vec2 vTexCoord;
layout(location=1) out vec4 vColor;

layout(push_constant) uniform PC
{
	mat3 transform;
};

void main()
{
	vTexCoord = texCoord_in;
	vColor = vec4(pow(color_in.rgb, vec3(2.2)), color_in.a);
	
	vec2 sPos = (transform * vec3(position_in, 1.0)).xy;
	
	gl_Position = vec4(sPos, 0.0, 1.0);
}
