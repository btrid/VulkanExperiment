
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

struct FrustomPoint
{
	vec4 m_ltn;
	vec4 m_rtn;
	vec4 m_lbn;
	vec4 m_rbn;
	vec4 m_ltf;
	vec4 m_rtf;
	vec4 m_lbf;
	vec4 m_rbf;
};

#define INVALID_LIGHT_INDEX uint(0xFFFFFFFF)