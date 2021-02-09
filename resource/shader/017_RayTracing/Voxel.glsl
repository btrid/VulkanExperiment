#ifndef VOXEL_H_
#define VOXEL_H_

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#if defined(USE_Voxel)

struct VoxelInfo
{
	uvec4 reso;
	uvec4 bottom_brick;
	uvec4 top_brick;
	uvec4 bottom;
	uvec4 top;
	uint material_size;

};

layout(set=USE_Voxel,binding=0, std140) uniform V0 {VoxelInfo u_info; };
layout(set=USE_Voxel,binding=1, scalar) buffer V1 { uvec4 b_top[]; };
layout(set=USE_Voxel,binding=2, scalar) buffer V2 { uvec4 b_bottom[]; };
layout(set=USE_Voxel,binding=3, scalar) buffer V3 { uint b_material_counter; };
layout(set=USE_Voxel,binding=4, scalar) buffer V4 { uint b_material_id[]; };
layout(set=USE_Voxel,binding=5, scalar) buffer V5 { uint b_material[]; };
#endif

#endif // VOXEL_H_