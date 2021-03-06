#version 460

#extension GL_GOOGLE_include_directive : require
#define USE_Voxel 0
#include "Voxel.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/camera.glsl"

layout(points, invocations = 32) in;
layout(points, max_vertices = 2*64) out;

layout(location=1) in Vertex
{
	flat int VertexIndex;
}in_param[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};

layout(location=1) out Out
{
	vec3 Color;
}gs_out;


void main() 
{
	int vi = in_param[0].VertexIndex;

	if(vi >= b_leaf_data_counter.w) { return; }
	vec3 p = vec3(b_leaf_data[vi].pos)*4;
	uvec2 child = b_leaf_data[vi].bitmask;
	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;

	for(int g = 0; g < 2; g++)
	{
		int gi = gl_InvocationID*2+g;
		if(!isBitOn(child, gi)) { continue;}

		LeafNode leaf = b_leaf[b_leaf_data[vi].leaf_index+bitcount(child,gi)];
		gs_out.Color = abs(vec3(unpack_normal(leaf.normal)));
//		gs_out.Color = abs(leaf.normal);

		vec3 lp = vec3(gi%4, (gi/4)%4, gi/16);

		gl_Position = pv * vec4(p+lp, 1.);
		gl_PointSize = 10. / gl_Position.w;
		EmitVertex();
	}
	EndPrimitive();

}
