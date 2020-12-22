#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LDC 0
#include "LDC.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"


layout(points, invocations = 1) in;
layout(triangle_strip, max_vertices = 12) out;


layout(location=0)in gl_PerVertex
{
	vec4 gl_Position;
} gl_in[];

layout(location=1) in InData
{
	flat uint VertexIndex;
} gs_in[];

layout(location=0)out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1)out OutData
{
	vec3 Normal;
	flat uint VertexIndex;
	vec3 Position;
}gs_out;


uvec3 g_offset[3][3] = 
{
	{{1,0,0}, {0, 1, 0}, {1, 1, 0}},
	{{0,1,0}, {0, 0, 1}, {0, 1, 1}},
	{{0,0,1}, {1, 0, 0}, {1, 0, 1}},
};

void main()
{
	mat4 pv = u_camera[0].u_projection * u_camera[0].u_view;
	vec3 extent = u_info.m_aabb_max.xyz-u_info.m_aabb_min.xyz;
	vec3 voxel_size = extent / Voxel_Reso;

	uvec4 i30 = uvec4(b_dcv_index[gs_in[0].VertexIndex]);

	for(int i = 0; i < 3; i++)
	{
		if((i30.w & (1<<i)) == 0) continue;


		uvec3 i31 =  i30.xyz + g_offset[i][0];
		uvec3 i32 =  i30.xyz + g_offset[i][1];
		uvec3 i33 =  i30.xyz + g_offset[i][2];
		uint i0 = i30.x + i30.y*Voxel_Reso.x + i30.z*Voxel_Reso.x*Voxel_Reso.y;
		uint i1 = i31.x + i31.y*Voxel_Reso.x + i31.z*Voxel_Reso.x*Voxel_Reso.y;
		uint i2 = i32.x + i32.y*Voxel_Reso.x + i32.z*Voxel_Reso.x*Voxel_Reso.y;
		uint i3 = i33.x + i33.y*Voxel_Reso.x + i33.z*Voxel_Reso.x*Voxel_Reso.y;

		vec3 v0 = (vec3(b_dcv_vertex[i0])/255. + vec3(i30.xyz)) * voxel_size + u_info.m_aabb_min.xyz;
		vec3 v1 = (vec3(b_dcv_vertex[i1])/255. + vec3(i31.xyz)) * voxel_size + u_info.m_aabb_min.xyz;
		vec3 v2 = (vec3(b_dcv_vertex[i2])/255. + vec3(i32.xyz)) * voxel_size + u_info.m_aabb_min.xyz;
		vec3 v3 = (vec3(b_dcv_vertex[i3])/255. + vec3(i33.xyz)) * voxel_size + u_info.m_aabb_min.xyz;

		vec3 normal = normalize(cross(v1-v0, v2-v0));


		gl_Position = pv * vec4(v0, 1.0);
		gs_out.Normal = normal;
		gs_out.VertexIndex = gs_in[0].VertexIndex;
		gs_out.Position = v0;
		EmitVertex();
		gl_Position = pv * vec4(v1, 1.0);
		gs_out.Normal = normal;
		gs_out.VertexIndex = gs_in[0].VertexIndex;
		gs_out.Position = v1;
		EmitVertex();
		gl_Position = pv * vec4(v2, 1.0);
		gs_out.Normal = normal;
		gs_out.VertexIndex = gs_in[0].VertexIndex;
		gs_out.Position = v2;
		EmitVertex();
		gl_Position = pv * vec4(v3, 1.0);
		gs_out.Normal = normal;
		gs_out.VertexIndex = gs_in[0].VertexIndex;
		gs_out.Position = v3;
		EmitVertex();
		EndPrimitive();

	}

}
