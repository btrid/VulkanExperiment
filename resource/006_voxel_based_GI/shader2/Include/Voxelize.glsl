struct VoxelInfo
{
	vec4 u_area_min;
	vec4 u_area_max;
	vec4 u_cell_size;
	uvec4 u_cell_num;
};

#if defined(SETPOINT_VOXEL)
layout(std140, set=SETPOINT_VOXEL, binding = 0) uniform VoxelInfoUniform
{
	VoxelInfo u_voxel_info;
};
layout (set=SETPOINT_VOXEL, binding=1, r32ui) uniform uimage3D t_voxel_albedo;


ivec3 getVoxelIndex(/*in VoxelInfo info,*/ in vec3 pos) 
{
	return ivec3((pos - u_voxel_info.u_area_min.xyz) / u_voxel_info.u_cell_size.xyz);
}
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
layout(std140, set=SETPOINT_VOXEL_MODEL, binding = 0) buffer VoxelMaterialBuffer
{
	VoxelMaterial b_voxel_model_material[];
};
layout(std140, set=SETPOINT_VOXEL_MODEL, binding = 1) buffer VoxelMeshInfoBuffer
{
	VoxelMeshInfo b_voxel_model_mesh_info[];
};
#endif
