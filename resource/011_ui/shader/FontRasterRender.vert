#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#define USE_SYSTEM 0
#include <applib/System.glsl>

layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location = 1) out VS{
	vec2 texcoord;
}vsout;


void main()
{
	vec2 v = vec2(gl_VertexIndex/2, gl_VertexIndex%2)*2. - 1.;
	v *= vec2(16.) / u_system_data.m_resolution;

	gl_Position = vec4(v, 0., 1.);

	float width = 1. / 64.;
	vsout.texcoord = vec2(gl_VertexIndex/2 * width, gl_VertexIndex%2);
}
