#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#define USE_CULLING 0
#include <Culling.glsl>

layout(early_fragment_tests) in;
in vec4 gl_FragCoord;
out float gl_FragDepth;

layout(location = 1)in Vertex{
	flat int instance_ID;
}In;

void main()
{
	b_visible[In.instance_ID] = 1;
	gl_FragDepth = gl_FragCoord.z;

}