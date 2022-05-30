#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"


layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out Vertex{
	flat int VertexIndex;
}vs_out;

void main() 
{
	vs_out.VertexIndex = gl_VertexIndex;
	gl_Position = vec4(1.);
}
