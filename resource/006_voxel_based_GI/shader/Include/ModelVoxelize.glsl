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
