#ifndef BTR_UI_GLSL_
#define BTR_UI_GLSL_

#define UI_TOUCH_ON (1 << 0)
#define UI_TOUCH_HOLD (1 << 1)
#define UI_TOUCH_OFF (1 << 2)
#define UI_TOUCH_BOUNDARY_IN (1 << 3)
//#define UI_TOUCH_OFF (1 << 4)

#define UI_enable_bit (1 << 1)
#define is_visible (1 << 2)
#define is_sprite (1 << 3)
#define is_boundary (1 << 4)

struct UIGlobal
{
	uvec2 m_resolusion; // 解像度
};
struct UIScene
{
	uint m_is_disable_order;
};
#define Scene_is_disable_order (1<<0)

struct UIInfo
{
	uint m_object_num;
	uint m_node_num;
	uint m_sprite_num;
	uint m_boundary_num;

	uvec2 m_target_resolution;
	uint m_depth_max;
	uint _p13;
};

struct UIParam
{
	vec2 m_position_local; //!< 自分の場所
	vec2 m_size_local;
	vec4 m_color_local;
	uint m_user_id;
	uint m_flag;
	int m_depth;
	int m_texture_index;

	int m_parent_index;
	int m_child_index;
	int m_chibiling_index;
	uint _p23;
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
	uint _p;

	uint m_touch_callback;
	uint m_click_callback;
	uint m_release_callback;
	uint _p2;
};
struct UIAnimePlayInfo
{
	uint m_flag;
	int m_anime_target;
	float m_frame;
};
#define AnimePlayInfo_is_playing (1<<0)
#define AnimePlayInfo_is_1 (1<<1)
#define AnimePlayInfo_is_2 (1<<2)

struct UIEvent
{
	uint m_type;
	int m_value1;
	int m_value2;
	int m_value3;
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

layout(std430, set=USE_UI, binding=6) restrict buffer UIUserIDBuffer
{
	uint b_user_id[];
};
layout(std430, set=USE_UI, binding=7) restrict buffer UIEventBuffer
{
	UIEvent b_event[];
};

layout(std430, set=USE_UI, binding=8) restrict buffer UISceneBuffer
{
	UIScene b_scene;
};

layout(set=USE_UI, binding=9) uniform sampler2D tDiffuse[32];

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
	uint m_key_offset_num_type;	//!< オフセット16 数8 type8
};

#define	AnimeDataInfo_flag_is_enable (1 << 0)
#define	AnimeDataInfo_flag_is_erase (1 << 1)
#define	AnimeDataInfo_type_pos_xy (0)
#define	AnimeDataInfo_type_size_xy (1)
#define	AnimeDataInfo_type_color_rgba (2)
#define	AnimeDataInfo_type_disable_order (3)

#define getTargetIndex(_info) (_info.m_target_index_flag & 0x0000ffff)
#define getFlag(_info) ((_info.m_target_index_flag & 0xffff0000) >> 16)
#define getKeyOffset(_info) (_info.m_key_offset_num_type & 0x0000ffff)
#define getKeyNum(_info) ((_info.m_key_offset_num_type & 0x00ff0000) >> 16)
#define getKeyType(_info) ((_info.m_key_offset_num_type & 0xff000000) >> 24)
struct UIAnimeKey
{
	uint m_frame;
	uint m_flag;
	int	m_value;
	uint _p;
};
#define	AnimeKey_is_enable (1 << 0)
#define	AnimeKey_is_erase (1 << 1)
#define	AnimeKey_interp_switch (1 << 3)

struct UIAnimeWork
{
	float m_current_frame;
	uint m_current_index;
};

#ifdef USE_UI_ANIME
#define LIST_NUM 2
layout(std140, set=USE_UI_ANIME, binding=0) uniform UIAnimeInfoUniform 
{
	UIAnimeInfo u_anime_info;
};
layout(std430, set=USE_UI_ANIME, binding=1) buffer UIAnimeDataInfoBuffer 
{
	UIAnimeDataInfo m_list[];
}u_anime_data_info[LIST_NUM];

layout(std430, set=USE_UI_ANIME, binding=2) buffer UIAnimeKeyBuffer 
{
	UIAnimeKey b_anime_key[];
};
#endif


#endif // BTR_UI_GLSL_