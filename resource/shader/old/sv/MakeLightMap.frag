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

void main()
{
	gl_FragStencilRefARB = 254;
}