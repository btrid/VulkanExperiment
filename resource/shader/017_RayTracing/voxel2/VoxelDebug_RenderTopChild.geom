#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"

layout(points, invocations = 32) in;
layout(triangle_strip, max_vertices = 14*2) out;

const vec3 cube_strip[] = 
{
vec3(0.f, 1.f, 1.f),    // Front-top-left
vec3(1.f, 1.f, 1.f),    // Front-top-right
vec3(0.f, 0.f, 1.f),    // Front-bottom-left
vec3(1.f, 0.f, 1.f),    // Front-bottom-right
vec3(1.f, 0.f, 0.f),    // Back-bottom-right
vec3(1.f, 1.f, 1.f),    // Front-top-right
vec3(1.f, 1.f, 0.f),    // Back-top-right
vec3(0.f, 1.f, 1.f),    // Front-top-left
vec3(0.f, 1.f, 0.f),    // Back-top-left
vec3(0.f, 0.f, 1.f),    // Front-bottom-left
vec3(0.f, 0.f, 0.f),    // Back-bottom-left
vec3(1.f, 0.f, 0.f),    // Back-bottom-right
vec3(0.f, 1.f, 0.f),    // Back-top-left
vec3(1.f, 1.f, 0.f),    // Back-top-right
};

layout(location=1) in Vertex
{
	flat int VertexIndex;
}in_param[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};


void main() 
{
	int vi = in_param[0].VertexIndex;
	if(b_hashmap[vi] < 0) { return; }

	uvec2 child = b_interior[b_hashmap[vi]].bitmask;
	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;


	ivec3 reso = u_info.reso.xyz>>ivec3(2)>>ivec3(2);
	int x = vi % reso.x;
	int y = (vi / reso.x) % reso.y;
	int z = (vi / (reso.x * reso.y)) % reso.z;
	vec3 p = vec3(x, y, z)*16;

	for(int g = 0; g < 2; g++)
	{
		int gi = gl_InvocationID*2+g;
		if((child[gi/32] & (1<<(gi%32))) == 0) { continue;}
		vec3 lp = vec3(gi%4, (gi/4)%4, (gi/16)%4);
		lp *= 4;

		for(int i = 0; i < cube_strip.length(); i++)
		{
			gl_Position = pv * vec4(cube_strip[i]+p+lp, 1.);
			EmitVertex();
		}
		EndPrimitive();

	}

}
