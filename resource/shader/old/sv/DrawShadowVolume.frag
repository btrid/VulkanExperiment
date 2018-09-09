#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Shape.glsl"

#define USE_SV 0
#include "SV.glsl"

layout(early_fragment_tests) in;
layout(location = 0) out vec4 FragColor;
void main()
{
	FragColor = vec4(1., 0., 0., 1.);
}