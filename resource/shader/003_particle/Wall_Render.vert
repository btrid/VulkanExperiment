#version 460
#extension GL_GOOGLE_include_directive : require

#define SETPOINT_CAMERA 0
#include "btrlib/Camera.glsl"

#define Use_Fluid 1
#include "Fluid.glsl"

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};
layout(location = 1) out Vertex
{
	flat uint VertexIndex; 
}Out;

void main()
{
	Out.VertexIndex = gl_VertexIndex;
}
