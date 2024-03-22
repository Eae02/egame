#version 450 core

layout(set = 1, binding = 0) uniform texture2D inputImage;

layout(set = 0, binding = 0) uniform sampler imageSampler;

layout(set = 0, binding = 1) uniform ParamsUB
{
	vec4 threshold;
};

layout(location = 0) out vec4 color_out;

void main()
{
	vec4 color = texture(sampler2D(inputImage, imageSampler), gl_FragCoord.xy * vec2(2.0) / vec2(textureSize(sampler2D(inputImage, imageSampler), 0).xy));
	color_out = max(color - threshold, vec4(0.0));
}
