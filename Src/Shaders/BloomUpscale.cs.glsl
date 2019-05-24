#version 450 core

layout(local_size_x=32, local_size_y_id=0) in;

layout(binding=0) uniform sampler2D inputImage;

layout(rgba16f, binding=1) uniform image2D outputImage;

layout(push_constant) uniform PC
{
	vec2 pixelSize;
};

void main()
{
	vec4 color = texture(inputImage, (vec2(gl_GlobalInvocationID.xy) + vec2(0.5)) * pixelSize);
	color += imageLoad(outputImage, ivec2(gl_GlobalInvocationID.xy));
	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), color);
}
