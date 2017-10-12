#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#define USE_CULLING 0
#include <Culling.glsl>

layout(early_fragment_tests) in;

layout(location = 1)in Vertex{
	flat int objectID;
}In;



void main()
{
	b_visible[In.objectID] = 1;
//	bVisible[In.objectID].instanceCount = 1;
}