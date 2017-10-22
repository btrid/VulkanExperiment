

struct TileInfo
{
	uvec2 m_resolusion;
	uvec2 m_tile_num;
	uint m_tile_index_map_max;
	uint m_tile_buffer_max_num;
};
struct LightInfo
{
	uint m_max_num;
};
struct LightData
{
	vec4 m_pos;	//!< xyz:pos w:scale
	vec4 m_emissive;
};

#ifdef USE_LIGHT
layout(std140, set=USE_LIGHT, binding=0) uniform LightInfoUniform {
	LightInfo u_light_info;
};
layout(std140, set=USE_LIGHT, binding=1) uniform TileInfoUniform {
	TileInfo u_tile_info;
};
layout(std430, set=USE_LIGHT, binding=2) restrict buffer LightDataBuffer {
	LightData b_light[];
};
layout(std430, set=USE_LIGHT, binding=3) restrict buffer CounterBuffer {
	uint b_data_counter;
};
layout(std430, set=USE_LIGHT, binding=4) restrict buffer TileCounterBuffer {
	uint b_tile_data_counter[];
};
layout(std430, set=USE_LIGHT, binding=5) restrict buffer TileIndexBuffer {
	uint b_tile_data_map[];
};

#endif