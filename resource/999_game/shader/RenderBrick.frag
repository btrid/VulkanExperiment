#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>
#include </TriangleLL.glsl>

layout(location=1) in Transform{
	flat int Visible;
}FSIn;

//layout(early_fragment_tests) in;
layout(location = 0) out vec4 FragColor;

void main()
{	
	if(FSIn.Visible == 0){
		discard;
		return;
	}

	FragColor = vec4(vec3(1.), (FSIn.Visible == 0 ? 0. : 1.));
}