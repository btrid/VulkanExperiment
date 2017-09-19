#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require

#include </convertDimension.glsl>

#define SETPOINT_VOXEL 0
#define SETPOINT_VOXEL_MODEL 1
#include </Voxelize.glsl>

layout(location=1)in Transform{
	vec3 Position;
	uint DrawID;
}transform;

void main()
{
	
	ivec3 index = getVoxelIndex(transform.Position.xyz);
	{
		uint material_index = b_voxel_model_mesh_info[transform.DrawID].material_index;
		vec3 albedo = b_voxel_model_material[material_index].albedo.xyz;
		// 1<<11=2048 1<<10=1024 1<<9=512 1<<5=32
		// r=9,g=9,b=9,count=5
		uint packd_albedo = uint(albedo.r*64)<<23 | uint(albedo.g*64)<<14 | uint(albedo.b*64)<<5 | 1;
		imageAtomicAdd(t_voxel_albedo, index, packd_albedo);
	}

}