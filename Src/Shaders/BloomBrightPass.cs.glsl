#version 450 core

layout(local_size_x=32, local_size_y_id=0) in;

layout(binding=0) uniform sampler2D inputImage;

layout(rgba16f, binding=1) writeonly uniform image2D outputImage;

layout(push_constant) uniform PC
{
	vec4 threshold;
};

void main()
{
	vec4 color = vec4(0.0);
	for (int x = 0; x < 2; x++)
	{
		for (int y = 0; y < 2; y++)
		{
			color += texelFetch(inputImage, ivec2(gl_GlobalInvocationID.xy) * 2 + ivec2(x, y), 0);
		}
	}
	
	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), max((color / 4.0) - threshold, vec4(0.0)));
}
