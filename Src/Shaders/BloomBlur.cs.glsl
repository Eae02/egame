#version 450 core

layout(local_size_x=32, local_size_y_id=0) in;

layout(binding=0) uniform sampler2D inputImage;

layout(rgba16f, binding=1) writeonly uniform image2D outputImage;

const float kernel[] = float[] (0.382928, 0.241732, 0.060598, 0.005977, 0.000229);

layout(push_constant) uniform PC
{
	vec2 blurVector;
	vec2 pixelSize;
};

void main()
{
	vec2 texCoord = vec2(gl_GlobalInvocationID.xy) * pixelSize;
	vec3 color = texture(inputImage, texCoord).rgb * kernel[0];
	
	for (int i = 1; i < kernel.length(); i++)
	{
		color += texture(inputImage, texCoord + blurVector * i).rgb * kernel[i];
		color += texture(inputImage, texCoord - blurVector * i).rgb * kernel[i];
	}
	
	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1.0));
}
