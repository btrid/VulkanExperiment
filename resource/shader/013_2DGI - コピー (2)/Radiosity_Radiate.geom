#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

#define invocation_num 1
layout(points, invocations = invocation_num) in;
layout(triangle_strip, max_vertices = 2*Vertex_Num/invocation_num+2) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=1) in Data
{
	flat uint index;
	flat uint emissive;
	vec4 color;
} gs_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out OutData
{
	vec4 color;
}gs_out;
void main() 
{
	uint index = gs_in[0].index;
	vec4 color = gs_in[0].color * 16.;

	u16vec2 pos = b_vertex_array[index].pos;
	vec2 center = (pos / 1024.) * 2. - 1.;

	if(gs_in[0].emissive ==0)
	{
		// デバッグ用
		gl_Position = vec4(center, 0., 1.);
		gs_out.color = color;
		EmitVertex();

		gl_Position = vec4(center + 2./vec2(1024.), 0., 1.);
		gs_out.color = color;
		EmitVertex();

		gl_Position = vec4(center + vec2(2., 0.)/vec2(1024.), 0., 1.);
		gs_out.color = color;
		EmitVertex();

		EndPrimitive();
		return;

	}


	uint num = Vertex_Num / invocation_num;

	for(uint _i = 0; _i < num; _i++)
	{
		uint i = _i + num * gl_InvocationID;
		vec2 v1 = vec2(b_vertex_array[index].vertex[i]) / 1024.;
		gl_Position = vec4(v1*2. - 1., 0., 1.);
		gs_out.color = color;
		EmitVertex();

		gl_Position = vec4(center, 0., 1.);
		gs_out.color = color;
		EmitVertex();
	}
	vec2 v1 = vec2(b_vertex_array[index].vertex[(num * (gl_InvocationID+1)) % Vertex_Num]) / 1024.;
	gl_Position = vec4(v1*2. - 1., 0., 1.);
	gs_out.color = color;
	EmitVertex();

	gl_Position = vec4(center, 0., 1.);
	gs_out.color = color;
	EmitVertex();

	EndPrimitive();

}
