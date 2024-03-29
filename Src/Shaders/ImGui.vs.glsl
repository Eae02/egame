#version 450 core

layout(location=0) in vec2 position_in;
layout(location=1) in vec2 texCoord_in;
layout(location=2) in vec4 color_in;

layout(location=0) out vec4 vColor;
layout(location=1) out vec2 vTexCoord;

layout(push_constant) uniform PC
{
	vec2 uScale;
};

void main()
{
	vColor = vec4(pow(color_in.rgb, vec3(2.2)), color_in.a);
	vTexCoord = vec2(texCoord_in.x, texCoord_in.y);
	
	vec2 scaledPos = position_in * uScale;
	gl_Position = vec4(scaledPos.x - 1.0, 1.0 - scaledPos.y, 0, 1);
}
