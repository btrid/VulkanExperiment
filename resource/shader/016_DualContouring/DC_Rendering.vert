#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LDC 0
#include "LDC.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out VSVertex
{
	flat uvec3 CellID;
}Out;

void main()
{
	int face = gl_VertexIndex/3;
	int component = gl_VertexIndex%3;
	uint index = b_dcv_index[face][component];
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4(b_dcv_vertex[index], 1.0);

}
