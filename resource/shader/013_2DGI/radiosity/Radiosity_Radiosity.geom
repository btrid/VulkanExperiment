#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(points, invocations = 1) in;
layout(line_strip, max_vertices = 2) out;

layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=1) in InData
{
	flat uint vertex_index;
} gs_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out OutData
{
	flat vec3 color;
}gs_out;

void main()
{
	gl_Layer = int(u_radiosity_info.frame);

	u16vec4 pos = b_segment[gs_in[0].vertex_index].pos;
	uvec2 index = pos.xz + pos.yw * u_gi2d_info.m_resolution.xx;

	uint64_t radiance1 = b_radiance[index.x + (Bounce_Num%2)*u_gi2d_info.m_resolution.x*u_gi2d_info.m_resolution.y];
	uint64_t radiance2 = b_radiance[index.y + (Bounce_Num%2)*u_gi2d_info.m_resolution.x*u_gi2d_info.m_resolution.y];
	dvec3 rad1_d3 = dvec3(radiance1&ColorMask, (radiance1>>21)&ColorMask, (radiance1>>42)&ColorMask);
	dvec3 rad2_d3 = dvec3(radiance2&ColorMask, (radiance2>>21)&ColorMask, (radiance2>>42)&ColorMask);
	vec3 rad = vec3(rad1_d3 / 1024.) * vec3(getRGB(b_fragment[index.x]));
	rad = rad + vec3(rad2_d3 / 1024.) * vec3(getRGB(b_fragment[index.y]));

	if(dot(rad, vec3(1.)) <= 0.0005){ return; }

	vec4 vertex = ((vec4(pos) + vec4(0.5)) / vec4(u_gi2d_info.m_resolution.xyxy)) * 2. - 1.;

	gl_Position = vec4(vertex.xy, 0., 1.);
	gs_out.color = rad;
	EmitVertex();

	gl_Position = vec4(vertex.zw, 0., 1.);
	gs_out.color = rad;
	EmitVertex();
	EndPrimitive();

}
