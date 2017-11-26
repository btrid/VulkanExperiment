#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#define USE_UI 0
#include <UI.glsl>
layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};
/*layout(location = 1) out PerVertex{
	vec4 color;
}vertex;
*/
void main()
{
	vec2 v = vec2(gl_VertexIndex%2, gl_VertexIndex/2);
	vec2 center = b_param[gl_InstanceIndex].m_position_local;
	vec2 scale = (v*2.-1.)*b_param[gl_InstanceIndex].m_size_local*0.5;
	v = ((center+scale) / vec2(u_global.m_resolusion))*2.-1.;
	gl_Position = vec4(v, 0., 1.);
}
