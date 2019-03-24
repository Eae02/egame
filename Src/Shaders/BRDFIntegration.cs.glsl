#version 450 core
#extension GL_GOOGLE_include_directive : enable

const int NUM_SAMPLES = 256;

layout(local_size_x=32, local_size_y=32) in;

layout(rg32f, binding=0) writeonly uniform image2D outputImage;

#include "IBLCommon.glh"

void main()
{
	vec2 texCoord = vec2(gl_GlobalInvocationID.xy) / vec2(imageSize(outputImage));
	
	float roughness = texCoord.y;
	float roughnessExp2 = roughness * roughness;
	float roughnessExp4 = roughnessExp2 * roughnessExp2;
	
	float nDotV = texCoord.x;
	
	vec3 V = vec3(sqrt(1.0 - nDotV * nDotV), 0.0, nDotV);
	
	vec2 sum = vec2(0.0);
	vec3 N = vec3(0.0, 0.0, 1.0);
	
	for (int i = 0; i < NUM_SAMPLES; i++)
	{
		vec2 Xi = Hammersley(i, NUM_SAMPLES);
		vec3 H  = ImportanceSampleGGX(Xi, mat3(1), roughnessExp4);
		vec3 L  = normalize(2.0 * dot(V, H) * H - V);
		
		float NdotL = max(L.z, 0.0);
		float NdotH = max(H.z, 0.0);
		float VdotH = max(dot(V, H), 0.0);
		
		if (NdotL > 0.0)
		{
			float G = GeometrySmith(vec3(0, 0, 1), V, L, roughnessExp2);
			float G_Vis = (G * VdotH) / (NdotH * nDotV);
			float Fc = pow(1.0 - VdotH, 5.0);
			
			sum += vec2(1.0 - Fc, Fc) * G_Vis;
		}
	}
	
	imageStore(outputImage, ivec2(gl_GlobalInvocationID.xy), vec4(sum / float(NUM_SAMPLES), 0.0, 0.0));
}
