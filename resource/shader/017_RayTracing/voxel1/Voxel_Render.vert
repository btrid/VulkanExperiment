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
	flat uint VertexIndex;
}vs_out;

void main() 
{
	vs_out.VertexIndex = gl_VertexIndex;
	gl_Position = vec4(1.);
/*	int x = gl_VertexIndex % u_info.reso.x;
	int y = (gl_VertexIndex / u_info.reso.x) % u_info.reso.y;
	int z = (gl_VertexIndex / u_info.reso.x / u_info.reso.y) % u_info.reso.z;
	vec3 p = vec3(x, y, z)+0.5;
	vec4 d = vec4(0.);
	d[0] = map(p);
	d[1] = map(p+1.);
	d[2] = map(p);
	d[3] = map(p+1.);

	gl_PointSize = 0.;
	gl_Position = vec4(1.);
	// 境界のみボクセル化する
	if(any(greaterThanEqual(d, vec4(0))) && any(lessThanEqual(d, vec4(0))))
	{
		gl_Position = u_camera[0].u_projection * u_camera[0].u_view * vec4(vec3(x, y, z), 1.);
		gl_PointSize = 1.;
	}
*/
}
