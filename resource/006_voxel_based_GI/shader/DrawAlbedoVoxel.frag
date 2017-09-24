#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <Voxelize.glsl>

layout(location=1)in Vertex{
	vec3 albedo;
}In;

//layout(early_fragment_tests) in;
//layout(post_depth_coverage) in;
layout(location = 0) out vec4 FragColor;

void main()
{	
	FragColor = vec4(In.albedo.rgb, 1.);
//	FragColor = vec4(1.);

}