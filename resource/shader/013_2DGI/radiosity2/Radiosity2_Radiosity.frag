#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity2 1
#include "GI2D.glsl"
#include "Radiosity2.glsl"


layout(location=1)in Data
{
	flat vec3 color;
}fs_in;

layout(location = 0) out f16vec4 FragColor;
void main()
{
	FragColor = f16vec4(fs_in.color*0.14, 0.0);
}
