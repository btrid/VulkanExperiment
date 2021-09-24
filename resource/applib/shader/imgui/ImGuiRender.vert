#version 450

#extension GL_GOOGLE_include_directive : require

layout(location = 0)in vec2 in_position;
layout(location = 1)in vec2 in_texcoord;
layout(location = 2)in vec4 in_color;

#define USE_SYSTEM 1
#include "applib/System.glsl"

layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location = 1) out PerVertex
{
	noperspective  vec2 texcoord;
	noperspective  vec4 color;
}vertex;

void main()
{
	vec2 p = vec2((in_position/u_system_data.m_resolution) *2. - 1.);
	gl_Position = vec4(p, 0., 1.);
	vertex.texcoord = in_texcoord;
	vertex.color = in_color;
	
}
