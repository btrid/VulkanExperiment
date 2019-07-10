#version 450
#extension GL_GOOGLE_include_directive : require
//#extension VK_KHR_shader_draw_parameters : require
#extension GL_ARB_shader_draw_parameters : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"
#include "Radiosity.glsl"

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Data
{
    flat vec3 color;
	flat uint vertex_id;
    flat uint frame;
} vs_out;

void main()
{

	uint vertex_offset = u_radiosity_info.vertex_max * gl_DrawIDARB;
    vs_out.color = b_vertex[vertex_offset+gl_InstanceIndex].radiance[(Bounce_Num%2)];
    vs_out.vertex_id = gl_InstanceIndex;
    vs_out.frame = gl_DrawIDARB;
    gl_Position = vec4(1.);
}
