#ifndef BTRLIB_LIGHT_GLSL
#define BTRLIB_LIGHT_GLSL

struct LightInfo
{
	uvec2 m_resolution;
	uvec2 m_tile_size;

	uvec2 m_tile_num;
    uint m_active_light_num;

};

struct LightParam
{
	vec4 m_position;
	vec4 m_emission;
};
struct LightLL
{
	uint next;
	uint light_index;
};

#ifdef USE_LIGHT
layout(set=USE_LIGHT, binding=0, std140) uniform LightInfoUniform {
	LightInfo u_light_info;
};
layout(set=USE_LIGHT, binding=1, std430) restrict buffer LightBuffer {
	LightParam b_light[];
};
layout(set=USE_LIGHT, binding=2, std430) restrict buffer LLHeadBuffer {
	uint b_light_LL_head[];
};
layout(set=USE_LIGHT, binding=3, std430) restrict buffer LLBuffer {
	LightLL b_light_LL[];
};
layout(set=USE_LIGHT, binding=4, std430) coherent restrict buffer LightCounter {
	uint b_light_count;
};
#endif
#define INVALID_LIGHT_INDEX uint(-1)

#endif