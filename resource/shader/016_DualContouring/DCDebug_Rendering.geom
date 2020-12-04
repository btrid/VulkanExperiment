#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LDC 0
#include "LDC.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"


layout(points, invocations = 1) in;
layout(line_strip, max_vertices = 2) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=1) in InData
{
	flat uint VertexIndex;
	flat uvec3 CellID;
} gs_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out OutData
{
	flat uint VertexIndex;
	flat uvec3 CellID;
}gs_out;


void main()
{

	gs_in[0].VertexIndex;

//	gl_Position = vec4(vertex.xy, 0., 1.);

	EmitVertex();

//	gl_Position = vec4(vertex.zw, 0., 1.);

	EmitVertex();
	EndPrimitive();

}
