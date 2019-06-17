#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Data
{
    flat vec3 color;
	flat uint vertex_index;
} vs_out;

void main()
{
	u16vec2 pos = b_vertex_array[gl_InstanceIndex].pos;

   	uint light_index = pos.x + pos.y * u_gi2d_info.m_resolution.x;
	uint light_offset = u_gi2d_info.m_resolution.x * u_gi2d_info.m_resolution.y;
	f16vec3 color = f16vec3(0.);
	for(int i = 0; i < 4; i++)
	{
		color += b_radiance_ex[light_index + light_offset * i];
	}

    vs_out.color = color * 0.1;
    vs_out.vertex_index = gl_InstanceIndex;
    gl_Position = vec4(1.);
}
