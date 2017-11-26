#ifndef BTR_UI_GLSL_
#define BTR_UI_GLSL_

struct UIGlobal
{
	uvec2 m_resolusion; // 解像度
};

struct UIInfo
{
	uint m_object_num;
	uint m_node_num;
	uint m_sprite_num;
	uint m_boundary_num;
};

struct UIParam
{
	vec2 m_position_local; //!< 自分の場所
	vec2 m_size_local;
	vec4 m_color_local;
	uint m_flag;
	int m_parent_index;
	int m_child_index;
	int m_depth;

	int m_chibiling_index;
	uint _p11;
	uint _p12;
	uint _p13;

	vec2 m_position_anime; //!< animationで移動オフセット
	vec2 m_size_anime;
	vec4 m_color_anime;
	uint m_flag_anime;
	uint _p21;
	uint _p22;
	uint _p23;

	uint m_name_hash;
	uint m_name_hash_;
	uint _p32;
	uint _p33;

};

#ifdef USE_UI
layout(std140, set=USE_UI, binding=0) uniform UIGlobalUniform 
{
	UIGlobal u_global;
};
layout(std140, set=USE_UI, binding=1) uniform UIInfoUniform 
{
	UIInfo u_info;
};
layout(std430, set=USE_UI, binding=2) restrict buffer UIParamBuffer 
{
	UIParam b_param[];
};

#endif


#endif // BTR_UI_GLSL_