#ifndef VOXEL_H_
#define VOXEL_H_

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#if defined(USE_Voxel)

struct VoxelInfo
{
	ivec4 reso;
	ivec4 bottom_brick;
	ivec4 top_brick;
	ivec4 bottom_brick_sqrt;
	ivec4 top_brick_sqrt;
	ivec4 bottom_reso;
	ivec4 top_reso;
	int material_size;

};

struct VoxelData
{
	int a;
};

layout(set=USE_Voxel,binding=0, std140) uniform V0 {VoxelInfo u_info; };
layout(set=USE_Voxel,binding=1, scalar) buffer V1 { uvec4 b_top[]; };
layout(set=USE_Voxel,binding=2, scalar) buffer V2 { uvec4 b_bottom[]; };
layout(set=USE_Voxel,binding=3, scalar) buffer V3 { int b_map_index_counter; };
layout(set=USE_Voxel,binding=4, scalar) buffer V4 { int b_map_index[]; };
layout(set=USE_Voxel,binding=5, scalar) buffer V5 { int b_data[]; };
#endif


float sdBox( vec3 p, vec3 b )
{
	vec3 d = abs(p) - b;
	return min(max(d.x,max(d.y,d.z)),0.0) + length(max(d,0.0));
}
float sdSphere( vec3 p, float s )
{
  return length( p ) - s;
}

float map(in vec3 p)
{
	float d = 9999999.;
	{
//		d = min(d, sdSphere(p-vec3(1000., 250., 1000.), 1000.));

		d = min(d, sdBox(p-vec3(500., 200., 700.), vec3(300.)));
		d = min(d, sdSphere(p-vec3(1200., 190., 1055.), 200.));
	}
	return d;
}

#endif // VOXEL_H_