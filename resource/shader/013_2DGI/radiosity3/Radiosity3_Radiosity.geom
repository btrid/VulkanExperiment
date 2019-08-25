#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity2 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity2.glsl"

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
	ivec4 reso = u_gi2d_info.m_resolution / 2;

	u16vec4 pos = b_segment[gs_in[0].vertex_index].pos;
	uvec2 index = pos.xz + pos.yw * reso.xx;

/*
	uint64_t radiance1 = b_radiance[index.x + (Bounce_Num%2)*reso.x*reso.y];
	uint64_t radiance2 = b_radiance[index.y + (Bounce_Num%2)*reso.x*reso.y];
	dvec3 rad1_d3 = dvec3(radiance1&ColorMask, (radiance1>>21)&ColorMask, (radiance1>>42)&ColorMask);
	dvec3 rad2_d3 = dvec3(radiance2&ColorMask, (radiance2>>21)&ColorMask, (radiance2>>42)&ColorMask);
	vec3 rad = vec3(rad1_d3 / 1024.) * vec3(b_albedo[index.x].xyz);
	rad = rad + vec3(rad2_d3 / 1024.) * vec3(b_albedo[index.y].xyz);
*/

	uint radiance1 = b_radiance[index.x + (Bounce_Num%2)*reso.x*reso.y];
	uint radiance2 = b_radiance[index.y + (Bounce_Num%2)*reso.x*reso.y];
	vec3 rad1_d3 = vec3(radiance1&ColorMask, (radiance1>>10)&ColorMask, (radiance1>>20)&ColorMask);
	vec3 rad2_d3 = vec3(radiance2&ColorMask, (radiance2>>10)&ColorMask, (radiance2>>20)&ColorMask);
	vec3 rad = vec3(rad1_d3 / 128.) * vec3(b_albedo[index.x].xyz);
	rad = rad + vec3(rad2_d3 / 128.) * vec3(b_albedo[index.y].xyz);

	if(dot(rad, vec3(1.)) <= 0.0005){ return; }

	vec4 vertex = ((vec4(pos) + vec4(0.5)) / vec4(reso.xyxy)) * 2. - 1.;

	gl_Position = vec4(vertex.xy, 0., 1.);
	gs_out.color = rad;
	EmitVertex();

	gl_Position = vec4(vertex.zw, 0., 1.);
	gs_out.color = rad;
	EmitVertex();
	EndPrimitive();

}
