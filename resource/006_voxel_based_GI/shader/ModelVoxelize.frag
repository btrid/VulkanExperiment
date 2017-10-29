#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/ConvertDimension.glsl>

#define USE_VOXEL 0
#include <btrlib/Voxelize/Voxelize.glsl>
#define USE_VOXELIZE 1
#define SETPOINT_VOXEL_MODEL 2
#include </ModelVoxelize.glsl>


layout(location=1)in Transform{
	vec3 Position;
	vec3 Albedo;
}transform;

void main()
{
	VoxelInfo info = u_voxel_info;
	ivec3 index = getVoxelIndex(info, transform.Position.xyz);
	{
		vec3 albedo = transform.Albedo;
		albedo = vec3(1.);
		// 1<<11=2048 1<<10=1024 1<<9=512 1<<5=32
		// r=9,g=9,b=9,count=5
		uint packd_albedo = uint(albedo.r*64)<<23 | uint(albedo.g*64)<<14 | uint(albedo.b*64)<<5 | 1;
		imageAtomicAdd(t_voxel_albedo, index, packd_albedo);
	}

}