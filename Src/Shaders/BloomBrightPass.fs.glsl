#version 450 core

layout(binding=0) uniform sampler2D inputImage;

layout(push_constant) uniform PC
{
	vec4 threshold;
};

layout(location=0) out vec4 color_out;

void main()
{
	vec4 color = texture(inputImage, gl_FragCoord.xy * vec2(2.0) / vec2(textureSize(inputImage, 0).xy));
	color_out = max(color - threshold, vec4(0.0));
}
