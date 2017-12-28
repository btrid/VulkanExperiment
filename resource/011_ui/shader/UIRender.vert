#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#define USE_UI 0
#include <UI.glsl>
layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location = 1) out PerVertex{
	vec4 color;
	vec2 uv;
	flat int texture_index;
}vertex;

void main()
{
	vec2 v = vec2(gl_VertexIndex%2, gl_VertexIndex/2);
	vec2 center = b_work[gl_InstanceIndex].m_position;
	vec2 scale = (v*2.-1.)*b_work[gl_InstanceIndex].m_size*0.5;
	v = ((center+scale) / vec2(u_global.m_resolusion))*2.-1.;
	gl_Position = vec4(v, 0., 1.);

	vertex.color = b_work[gl_InstanceIndex].m_color;
	vertex.uv = vec2(gl_VertexIndex%2, 1.-gl_VertexIndex/2);
	vertex.texture_index = b_param[gl_InstanceIndex].m_texture_index;
}
