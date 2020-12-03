#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LDC 0
#include "LDC.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"
out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

layout(location=1) out VSVertex
{
	flat uvec3 CellID;
}Out;

void main()
{
	int z = gl_VertexIndex.x/64/64;
	int y = (gl_VertexIndex.x/64) % 64;
	int x = gl_VertexIndex.x % 64;
	gl_PointSize = 1.;
	Out.CellID = uvec3(x, y, z);

//	int hash = b_dcv_hashmap[gl_VertexIndex];
	int hash = b_ldc_point_link_head[gl_VertexIndex];
//	int hash = b_ldc_counter;
//	int hash = b_dcv_counter;
	if(hash <= 0)
	{
		gl_Position = vec4(-1.);
	}
	else
	{
		gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4(x, y, z, 1.0);
	}


}
