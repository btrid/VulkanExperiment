#ifndef BTR_UI_GLSL_
#define BTR_UI_GLSL_

#define UI_TOUCH_ON 1
#define UI_TOUCH_HOLD 2
#define UI_TOUCH_OFF 4

#define UI_enable_bit (1 << 1)
#define is_visible (1 << 2)
#define is_sprite (1 << 3)
#define is_boundary (1 << 4)

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

	uint m_depth_max;
	uint _p11;
	uint _p12;
	uint _p13;
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
struct UIWork
{
	vec2 m_position;
	vec2 m_size;
	vec4 m_color;
};

struct UIBoundary
{
	uint m_flag;
	float m_touch_time;
	uint m_param_index;
	uint m_callback_id;
//	vec2 m_touch_pos;
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
layout(std430, set=USE_UI, binding=3) restrict buffer UIWorkBuffer 
{
	UIWork b_work[];
};
layout(std430, set=USE_UI, binding=4) restrict buffer UIBoundaryBuffer 
{
	UIBoundary b_boundary[];
};

#endif


#endif // BTR_UI_GLSL_