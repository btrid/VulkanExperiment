#ifndef MOVABLE_GLSL_
#define MOVABLE_GLSL_

#include <btrlib/Common.glsl>

struct MovableInfo
{
	uint m_data_size; // doublebufferのシングルバッファ分
	uint m_active_counter;
};

struct MovableData
{
	uint64_t packed_pos;
	float scale;
	uint type;
};

struct TileInfo
{
	uvec2 m_tile_num;
	uvec2 m_resolusion;
	uint m_tile_count_max;
};

uint64_t packMovablePos(in vec3 pos)
{
	// 28, 8, 28
	uvec3 shift = uvec3(36, 28, 0);
	vec3 rate = vec3(10., 1., 10.);
	uvec3 u = uvec3(pos*rate) << shift;
	uint64_t packed_pos = u.x | u.y | u.z;
	return packed_pos;
}
vec3 unpackMovablePos(in uint64_t packed_pos)
{
	uvec3 shift = uvec3(36, 28, 0);
	vec3 rate = vec3(10., 1., 10.);
	vec3 pos = vec3(u64vec3(packed_pos) >> shift) / rate;
	return pos;
}

#ifdef USE_MOVABLE
layout(std430, set=USE_MOVABLE, binding=0) restrict buffer MovableInfoBuffer {
	MovableInfo b_movable_info;
};
layout(std430, set=USE_MOVABLE, binding=1) restrict buffer MovableDataBuffer {
	MovableData b_movable_data[];
};
layout(std430, set=USE_MOVABLE, binding=2) restrict buffer TileInfoBuffer {
	TileInfo b_tile_info;
};
layout(std430, set=USE_MOVABLE, binding=3) restrict buffer TileCountBuffer {
	uint b_tiled_count[];
};
layout(std430, set=USE_MOVABLE, binding=4) restrict buffer TileIndexBuffer {
	uint b_tiled_index[];
};

#endif

#endif