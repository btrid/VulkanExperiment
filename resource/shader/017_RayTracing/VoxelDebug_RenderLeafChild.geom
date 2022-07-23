#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"

layout(points, invocations = 1) in;
layout(triangle_strip, max_vertices = 14) out;

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
	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;

	int vi = in_param[0].VertexIndex/64/64;
	if(b_hashmap[vi] < 0) { return; }

	ivec3 reso = u_info.reso.xyz>>ivec3(2)>>ivec3(2);
	int x = (vi) % reso.x;
	int y = (vi / reso.x) % reso.y;
	int z = (vi / (reso.x * reso.y)) % reso.z;
	vec3 p = vec3(x, y, z)*64;


	InteriorNode node = b_interior[b_hashmap[vi]];
	int b = (in_param[0].VertexIndex/64)%64;

	if(!isBitOn(node.bitmask, b)){ return; }

	p += vec3(b%4, (b/4)%4, (b/16)%4)*16;

	node = b_interior[node.child + bitcount(node.bitmask, b)];
	b = in_param[0].VertexIndex%64;

	if(!isBitOn(node.bitmask, b)) { return;}

	LeafNode leaf = b_leaf[node.child+bitcount(node.bitmask,b)];
	vec3 c = vec3(abs(unpack_normal(leaf.normal)));
	p += vec3(b%4, (b/4)%4, (b/16)%4)*4;

	for(int i = 0; i < cube_strip.length(); i++)
	{
		gl_Position = pv * vec4(cube_strip[i]*4+p, 1.);
		gs_out.Color = c;
		EmitVertex();
	}
	EndPrimitive();

}