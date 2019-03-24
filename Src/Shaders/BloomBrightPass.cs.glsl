#version 450 core

layout(local_size_x=32, local_size_y=32) in;

layout(binding=0) uniform sampler2D inputImage;

layout(rgba16f, binding=1) writeonly uniform image2D outputImage;

layout(push_constant) uniform PC
{
	vec4 threshold;
	vec2 pixelSize;
};

void main()
{
	vec4 color = texture(inputImage, (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) * pixelSize);
	color = max(color - threshold, vec4(0.0));
	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), color);
}
