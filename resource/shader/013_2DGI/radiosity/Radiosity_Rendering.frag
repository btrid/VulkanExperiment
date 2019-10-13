#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"


layout(location=1)in Data
{
	vec2 texcoord;
}fs_in;

layout(location = 0) out f16vec4 FragColor;
void main()
{
	FragColor = f16vec4(0.);
	for(int i = 0; i < u_radiosity_info.frame_max; i++)
	{
		FragColor = fma(f16vec4(texture(s_radiosity[i], fs_in.texcoord)), f16vec4(5.), FragColor);
	}
}
