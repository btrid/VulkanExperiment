#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_PM 0
#include "PM.glsl"

layout(location=1) in Transform
{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

void main()
{
	
	uvec3 bv_index3d = uvec3(transform.Position.xyz / u_pm_info.num0.xyz);
	uvec3 bv_bit = bv_index3d%4;
	bv_index3d = bv_index3d/4;
	uint bv_index1d = bv_index3d.x+bv_index3d.y*u_pm_info.num0.x+bv_index3d.z*u_pm_info.num0.x*u_pm_info.num0.y;
	atomicOr(b_voxel_map[bv_index1d], 1ul<<(bv_bit.x+bv_bit.y*4+bv_bit.z*16));

}