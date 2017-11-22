#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <UI.glsl>
layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};

void main()
{
	gl_Position = vec4(gl_VertexIndex%2, gl_VertexIndex/2, 0., 1.);
}
