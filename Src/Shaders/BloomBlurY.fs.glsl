#version 450 core

layout(binding=0) uniform sampler2D inputImage;

const float kernel[] = float[] (0.382928, 0.241732, 0.060598, 0.005977, 0.000229);

layout(location=0) out vec4 color_out;

void main()
{
	vec2 pixelSize = 1.0 / vec2(textureSize(inputImage, 0).xy);
	vec2 texCoord = (gl_FragCoord.xy - vec2(0.5)) * pixelSize;
	vec3 color = texture(inputImage, texCoord).rgb * kernel[0];
	
	for (int i = 1; i < kernel.length(); i++)
	{
		color += texture(inputImage, texCoord + vec2(0, pixelSize.y * i * 2)).rgb * kernel[i];
		color += texture(inputImage, texCoord - vec2(0, pixelSize.y * i * 2)).rgb * kernel[i];
	}
	
	color_out = vec4(color, 1.0);
}
