#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"

layout(points, invocations = 32) in;
layout(triangle_strip, max_vertices = 14*2) out;

layout(location=1) in Vertex
{
	flat int VertexIndex;
}in_param[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};
layout(location=1) out Out
{
	vec3 Color;
}gs_out;


void main() 
{
	int vi = in_param[0].VertexIndex;
	if(b_hashmap[vi] < 0) { return; }

	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;

	InteriorNode top = b_interior[b_hashmap[vi]];
	if(top.child < 0){ return; }

	ivec3 reso = u_info.reso.xyz>>ivec3(2)>>ivec3(2);
	int x = vi % reso.x;
	int y = (vi / reso.x) % reso.y;
	int z = (vi / (reso.x * reso.y)) % reso.z;
	vec3 p = vec3(x, y, z)*16;

	for(int g = 0; g < 2; g++)
	{
		int gi = gl_InvocationID*2+g;
		if(!isBitOn(top.bitmask, gi)) { continue;}
		vec3 lp = vec3(gi%4, (gi/4)%4, (gi/16)%4);
		lp *= 4;

		for(int i = 0; i < cube_strip.length(); i++)
		{
			gl_Position = pv * vec4(cube_strip[i]*4+p+lp, 1.);
			gs_out.Color = vec3(gi%4, (gi/4)%4, (gi/16)%4) / 4.;
			EmitVertex();
		}
		EndPrimitive();

	}

}
