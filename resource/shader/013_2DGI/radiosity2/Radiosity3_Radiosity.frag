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
	uvec2 index = uvec2(gl_FragCoord.xy);
	vec3 rad = fs_in.color * vec3(b_albedo[index.x + index.y*u_gi2d_info.m_resolution.x/2].xyz);
	FragColor = f16vec4(fs_in.color, 0.0);
}
