
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


#define INVALID_LIGHT_INDEX uint(0xFFFFFFFF)