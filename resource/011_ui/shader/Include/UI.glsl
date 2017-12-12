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
};
struct UIAnimePlayInfo
{
	int m_anime_target;
	float m_frame;
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
layout(std430, set=USE_UI, binding=3) restrict buffer UIBoundaryBuffer 
{
	UIBoundary b_boundary[];
};
layout(std430, set=USE_UI, binding=4) restrict buffer UIWorkBuffer 
{
	UIWork b_work[];
};
layout(std430, set=USE_UI, binding=5) restrict buffer UIAnimePlayBuffer
{
	UIAnimePlayInfo b_anime_play_info[8];
};

#endif


struct UIAnimeInfo
{
	uint m_anime_num;
	uint m_anime_max_frame;
};

struct UIAnimeDataInfo
{
	uint m_target_hash_low;
	uint m_target_hash_hi;
	uint m_target_index_flag;
	uint m_key_offset_num;	//!< オフセットと数
};

#define	key_is_enable (1 << 0)
#define	key_is_erase (1 << 1)
#define	key_pos_xy (1 << 2)
#define	key_size_xy (1 << 3)
#define	key_color_rgba (1 << 4)

#define getTargetIndex(_info) (_info.m_target_index_flag & 0x0000ffff)
#define getFlag(_info) ((_info.m_target_index_flag & 0xffff0000) >> 16)
#define getKeyOffset(_info) (_info.m_key_offset_num & 0x0000ffff)
#define getKeyNum(_info) ((_info.m_key_offset_num & 0xffff0000) >> 16)
struct UIAnimeKey
{
	uint m_frame;
	uint m_flag;
	int	m_value;
	uint _p;
};

struct UIAnimeWork
{
	float m_current_frame;
	uint m_current_index;
};

#ifdef USE_UI_ANIME
layout(std140, set=USE_UI_ANIME, binding=0) uniform UIAnimeInfoUniform 
{
	UIAnimeInfo u_anime_info;
};
layout(std430, set=USE_UI_ANIME, binding=1) buffer UIAnimeDataInfoBuffer 
{
	UIAnimeDataInfo u_anime_data_info[];
};
layout(std430, set=USE_UI_ANIME, binding=2) buffer UIAnimeKeyBuffer 
{
	UIAnimeKey b_anime_key[];
};
/*layout(std430, set=USE_UI_ANIME, binding=3) buffer UIAnimeWorkBuffer
{
	UIAnimeWork b_anime_work[];
};
*/
#endif


#endif // BTR_UI_GLSL_