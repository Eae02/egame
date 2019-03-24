#version 450 core
#extension GL_GOOGLE_include_directive : enable

#include "IBLCommon.glh"
#include "IBLConvolve.glh"

layout(local_size_x=12, local_size_y=12, local_size_z=6) in;

layout(set=0, binding=0) uniform samplerCube environmentMap;

layout(rgba32f, binding=1) writeonly uniform imageCube outputImage;

layout(push_constant) uniform PushConstants
{
	float irradianceScale;
	float pixelSize;
};

void main()
{
	vec3 normal, tangent;
	GetConvolveDir(vec2(gl_GlobalInvocationID.xy) * pixelSize, gl_GlobalInvocationID.z, normal, tangent);
	vec3 up = cross(normal, tangent);
	
	const int SAMPLE_COUNT_H = 32;
	const int SAMPLE_COUNT_V = SAMPLE_COUNT_H * 4;
	
	vec3 color = vec3(0.0);
	
	for (int i = 0; i < SAMPLE_COUNT_V; i++)
	{
		float phi = (i * PI * 2) / float(SAMPLE_COUNT_V);
		vec3 rotatedRight = cos(phi) * tangent + sin(phi) * up;
		
		for (int j = 0; j < SAMPLE_COUNT_H; j++)
		{
			float theta = (j * PI * 0.5) / float(SAMPLE_COUNT_H);
			
			vec3 sampleVector = cos(theta) * normal + sin(theta) * rotatedRight;
			color += texture(environmentMap, sampleVector).rgb * cos(theta) * sin(theta);
		}
	}
	
	vec3 result = irradianceScale * PI * color / float(SAMPLE_COUNT_H * SAMPLE_COUNT_V);
	imageStore(outputImage, ivec3(gl_GlobalInvocationID), vec4(result, 0.0));
}
