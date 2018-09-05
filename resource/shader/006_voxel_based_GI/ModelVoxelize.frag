#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_VOXEL 0
#include "Voxelize.glsl"

#define USE_VOXELIZE 1
#define SETPOINT_VOXEL_MODEL 2
#include "ModelVoxelize.glsl"

#include "btrlib/ConvertDimension.glsl"


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
		uint packd_albedo = packRGB(albedo);
		imageAtomicAdd(t_voxel_albedo, index, packd_albedo);
	}

}