
#if defined(USE_VOXELIZE)
layout (set=USE_VOXELIZE, binding=0, r32ui) uniform uimage3D t_voxel_albedo;
#endif

#if defined(SETPOINT_VOXEL_MODEL)
struct VoxelMaterial
{
	vec4 albedo;
	vec4 emission;
};
struct VoxelMeshInfo
{
	uint material_index;
};
layout(std430, set=SETPOINT_VOXEL_MODEL, binding = 0) buffer VoxelMaterialBuffer
{
	VoxelMaterial b_voxel_model_material[];
};
layout(std430, set=SETPOINT_VOXEL_MODEL, binding = 1) buffer VoxelMeshInfoBuffer
{
	VoxelMeshInfo b_voxel_model_mesh_info[];
};
#endif

vec3 unpack(uint packed)
{
	uint count = (packed)&((1<<5)-1);
	if(count != 0)
	{
		uint packed_r = (packed>>23)&((1<<9)-1);
		uint packed_g = (packed>>14)&((1<<9)-1);
		uint packed_b = (packed>>5)&((1<<9)-1);
		vec3 unpacked = vec3(packed_r, packed_g, packed_b) / 64. / float(count);
		return unpacked;
	}
	return vec3(0.);

}

