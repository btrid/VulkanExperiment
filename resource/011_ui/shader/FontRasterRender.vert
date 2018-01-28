#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#define USE_SYSTEM 0
#include <applib/System.glsl>

#define USE_FONT_TEXTURE 1
#define USE_FONT_MAP 2
#include <Font.glsl>


layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location = 1) out VS{
	vec2 texcoord;
}vsout;


void main()
{
	vec2 v = b_glyph_map[gl_InstanceIndex].m_begin / vec2(u_system_data.m_resolution);
	vec2 offset = vec2(gl_VertexIndex/2 , gl_VertexIndex%2) * b_glyph_map[gl_InstanceIndex].m_size;
	offset /= u_system_data.m_resolution;
	v+=offset;
	gl_Position = vec4(v, 0., 1.);

	uint cache_No = b_glyph_map[gl_InstanceIndex].m_cache_index;
	vec2 texcoord_size = b_glyph_map[gl_InstanceIndex].m_size;
	vsout.texcoord = vec2(cache_No*36, 0.) + texcoord_size*vec2(gl_VertexIndex/2 , gl_VertexIndex%2);
	vsout.texcoord /= vec2(36*64, 36);
}
