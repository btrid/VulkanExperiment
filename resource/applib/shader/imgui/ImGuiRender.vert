#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

layout(location = 0)in vec2 in_position;
layout(location = 1)in vec2 in_texcoord;
layout(location = 2)in vec4 in_color;

#define USE_SYSTEM 1
#include <applib/System.glsl>

layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location = 1) out PerVertex{
	vec2 texcoord;
	vec4 color;
}vertex;

void main()
{
	vec2 p = vec2(in_position/ u_system_data.m_resolution *2. - 1.);
	gl_Position = vec4(p, 0., 1.);
//	gl_Position.y = 1.-gl_Position.y;
	vertex.texcoord = in_texcoord;
	vertex.color = in_color;
	
}
