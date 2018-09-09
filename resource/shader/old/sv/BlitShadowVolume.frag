#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Shape.glsl"

#define USE_SV 0
#define USE_SV_RENDER 1
#include "SV.glsl"

layout(location=1) in VS
{
	vec2 texcoord;
}fs_in;

layout(location = 0) out vec4 FragColor;
void main()
{
	FragColor = texture(s_color[0], fs_in.texcoord);
//	FragColor = vec4(0.);
}