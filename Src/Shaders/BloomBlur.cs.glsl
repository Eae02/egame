#version 450 core

layout(local_size_x=32, local_size_y_id=0) in;

layout(binding=0) uniform sampler2D inputImage;

layout(rgba16f, binding=1) writeonly uniform image2D outputImage;

const float kernel[] = float[] (0.20236, 0.179044, 0.124009, 0.067234, 0.028532);

layout(push_constant) uniform PC
{
	vec2 blurVector;
	vec2 pixelSize;
};

void main()
{
	vec2 texCoord = vec2(gl_GlobalInvocationID.xy) * pixelSize;
	vec3 color = texture(inputImage, texCoord).rgb * kernel[0];
	
	vec2 offset = blurVector;
	for (int i = 1; i < kernel.length(); i++)
	{
		color += texture(inputImage, texCoord + offset).rgb * kernel[i];
		color += texture(inputImage, texCoord - offset).rgb * kernel[i];
		offset += blurVector;
	}
	
	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), vec4(color, 1.0));
}
