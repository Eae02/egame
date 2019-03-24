#version 450 core
#extension GL_GOOGLE_include_directive : enable

#include "IBLCommon.glh"
#include "IBLConvolve.glh"

layout(local_size_x=12, local_size_y=12, local_size_z=6) in;

const int NUM_SAMPLES = 1024;

layout(binding=0) uniform samplerCube environmentMap;

layout(rgba32f, binding=1) writeonly uniform imageCube outputImage;

layout(push_constant) uniform PC
{
	float roughnessExp4; //roughness ^ 4
	float irradianceScale;
	float pixelSize;
};

void main()
{
	vec3 normal, tangent;
	GetConvolveDir(vec2(gl_GlobalInvocationID.xy) * pixelSize, gl_GlobalInvocationID.z, normal, tangent);
	
	mat3 tbnMatrix = mat3(tangent, cross(normal, tangent), normal);
	
	float totalWeight = 0.0;
	vec3 pfColor = vec3(0.0);
	
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		vec2 Xi = Hammersley(i, NUM_SAMPLES);
		vec3 H = ImportanceSampleGGX(Xi, tbnMatrix, roughnessExp4);
		vec3 L = normalize(2.0 * dot(normal, H) * H - normal);
		
		float NdotL = dot(normal, L);
		if (NdotL > 0.0)
		{
			pfColor += texture(environmentMap, L).rgb * NdotL;
			totalWeight += NdotL;
		}
	}
	
	pfColor *= irradianceScale / totalWeight;
	imageStore(outputImage, ivec3(gl_GlobalInvocationID), vec4(pfColor, 0.0));
}
