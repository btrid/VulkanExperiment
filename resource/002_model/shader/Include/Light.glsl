
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

#ifdef USE_LIGHT
layout(set=USE_LIGHT, binding=0, std140) uniform LightInfoUniform {
	LightInfo u_light_info;
};
layout(set=USE_LIGHT, std140, binding=1) uniform FrustomUniform {
	FrustomPoint u_frustom;
};

layout(set=USE_LIGHT, std430, binding=8) readonly restrict buffer LightBuffer {
	LightParam b_light[];
};
layout(set=USE_LIGHT, std430, binding=9) restrict buffer LLHeadBuffer {
	uint b_LLhead[];
};
layout(set=USE_LIGHT, std430, binding=10) writeonly restrict buffer LLBuffer {
	LightLL b_light_LL[];
};
layout(set=USE_LIGHT, std430, binding=11) coherent restrict buffer LightCounter {
	uint b_light_count;
};
#endif
