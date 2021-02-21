#ifndef VOXEL_H_
#define VOXEL_H_

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
// #extension GL_KHR_shader_subgroup : require
#extension GL_KHR_shader_subgroup_ballot : require
#if defined(USE_Voxel)

struct VoxelInfo
{
	ivec4 reso;

};

struct InteriorNode
{
	uvec2 bitmask;
	uint child;
};
struct LeafNode
{
	uint16_t normal;
	uint32_t albedo;
};

struct LeafData
{
	uvec2 bitmask;
	u16vec4 pos_index;
};

layout(set=USE_Voxel,binding=0, std140) uniform V0 {VoxelInfo u_info; };
layout(set=USE_Voxel,binding=1, scalar) buffer V1 { int b_hashmap[]; };
layout(set=USE_Voxel,binding=2, scalar) buffer V2 { ivec4 b_interior_counter[]; };
layout(set=USE_Voxel,binding=3, scalar) buffer V3 { int b_leaf_counter; };
layout(set=USE_Voxel,binding=4, scalar) buffer V4 { InteriorNode b_interior[]; };
layout(set=USE_Voxel,binding=5, scalar) buffer V5 { LeafNode b_leaf[]; };
layout(set=USE_Voxel,binding=6, scalar) buffer V6 { ivec4 b_leaf_data_counter; };
layout(set=USE_Voxel,binding=7, scalar) buffer V7 { LeafData b_leaf_data[]; };
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

float sdf(in vec3 p)
{
	float d = 9999999.;
	{
//		d = min(d, sdSphere(p-vec3(1000., 250., 1000.), 1000.));

		d = min(d, sdBox(p-vec3(500., 200., 700.), vec3(300.)));
		d = min(d, sdSphere(p-vec3(1200., 190., 1055.), 200.));
	}
	return d;
}

bool map(in vec3 p)
{
//	vec3 p = vec3(gl_GlobalInvocationID.xyz)+0.5;
	float d = sdf(p-0.5);
	for(int z = 0; z < 2; z++)
	for(int y = 0; y < 2; y++)
	for(int x = 0; x < 2; x++)
	{
		if(z==0 && y==0 && x==0){continue;}
		if(sign(d) != sign(sdf(p + vec3(x, y, z)-0.5)))
		{
			return true;
		}
	}
	return false;
}
#endif // VOXEL_H_