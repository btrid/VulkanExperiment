#ifndef WORLEYNOISE_HEADER_
#define WORLEYNOISE_HEADER_

#ifdef USE_WORLEYNOISE
layout(set=USE_WORLEYNOISE, binding=0) uniform sampler3D s_worleynoise_map;
layout(set=USE_WORLEYNOISE, binding=10, r8ui) uniform uimage3D i_worleynoise_map;

layout(set=USE_WORLEYNOISE, binding=20, std430) restrict buffer WN_SeedBuffer {
	ivec4 b_seed[];
};
layout(set=USE_WORLEYNOISE, binding=21, std430) restrict buffer WN_TileCounter {
	uint b_tile_counter[];
};
layout(set=USE_WORLEYNOISE, binding=22, std430) restrict buffer WN_TileDataBuffer {
	uint b_tile_buffer[];
};

#define TILE_SIZE 32
#define radius  27.
#endif // USE_WORLEYNOISE
#endif // WORLEYNOISE_HEADER_