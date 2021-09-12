#version 460

#extension GL_GOOGLE_include_directive : require
#define SETPOINT_CAMERA 0
#include "btrlib/camera.glsl"


layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out Vertex
{
	flat uint VertexIndex;
}
vs_out;

void main() 
{
//	vs_out.VertexIndex = gl_VertexIndex;
	gl_Position = vec4(1.);
}
