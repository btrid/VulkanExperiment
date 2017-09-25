#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
#pragma optionNV(strict on)

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/convertDimension.glsl>

#define SETPOINT_VOXEL 0
#include <btrlib/Voxelize/Voxelize.glsl>
#define SETPOINT_VOXEL_MODEL 1
#include </ModelVoxelize.glsl>

layout(location=1)in Transform{
	vec3 Position;
	vec3 Albedo;
}transform;

void main()
{
	
	ivec3 index = getVoxelIndex(transform.Position.xyz);
	{
		vec3 albedo = transform.Albedo;
		// 1<<11=2048 1<<10=1024 1<<9=512 1<<5=32
		// r=9,g=9,b=9,count=5
		uint packd_albedo = uint(albedo.r*64)<<23 | uint(albedo.g*64)<<14 | uint(albedo.b*64)<<5 | 1;
		imageAtomicAdd(t_voxel_albedo, index, packd_albedo);
	}

}