#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"

layout(points, invocations = 1) in;
layout(triangle_strip, max_vertices = 14) out;

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

	if(vi >= b_leaf_data_counter.w) { return; }
	vec3 p = vec3(b_leaf_data[vi].pos_index.xyz);

//	ivec3 reso = u_info.reso.xyz;
//	int x = vi % reso.x;
//	int y = (vi / reso.x) % reso.y;
//	int z = (vi / reso.x / reso.y) % reso.z;
	float scale = 1<<2;

	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;

	for(int i = 0; i < cube_strip.length(); i++)
	{
		gl_Position = pv * vec4((cube_strip[i]+p)*scale, 1.);
		EmitVertex();
	}
	EndPrimitive();
}
