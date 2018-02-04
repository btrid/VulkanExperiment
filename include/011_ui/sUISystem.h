#pragma once

#include <vector>
#include <array>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>
#include <btrlib/rTexture.h>

#include <011_ui/cerealDefine.h>
struct UIGlobal
{
	uvec2 m_resolusion; // 解像度
};
struct UIScene
{
	uint32_t m_is_disable_order; // 操作無効中
	uint32_t m_parent_user_id;
};
struct UIInfo
{
	uint m_object_num; // node + sprite + boundary
	uint m_node_num;
	uint m_sprite_num;
	uint m_boundary_num;

	uvec2 m_target_resolution;
	uint m_depth_max;
	uint _p13;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(
			CEREAL_NVP(m_object_num),
			CEREAL_NVP(m_node_num),
			CEREAL_NVP(m_sprite_num),
			CEREAL_NVP(m_boundary_num),
			CEREAL_NVP(m_target_resolution),
			CEREAL_NVP(m_depth_max)
		);
	}

};

enum UIFlagBit
{
	is_enable = 1 << 1,
	is_visible = 1 << 2,
	is_sprite = 1 << 3,
	is_boundary = 1 << 4,

	// for tool
	is_select = 1 << 30,	//!< 選択中
	is_trash = 1 << 31,		//!< 破棄済み
};
struct UIObject
{
	vec2 m_position_local; //!< 自分の場所
	vec2 m_size_local;
	vec4 m_color_local;

	int32_t m_user_id; //!< イベント等のターゲット用ID
	uint32_t m_flag;
	int32_t m_depth;
	uint32_t m_texture_index;

	int32_t m_parent_index;
	int32_t m_child_index;
	int32_t m_sibling_index;
	uint32_t _p23;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(
			CEREAL_NVP(m_position_local),
			CEREAL_NVP(m_size_local),
			CEREAL_NVP(m_color_local),

			CEREAL_NVP(m_user_id),
			CEREAL_NVP(m_flag),
			CEREAL_NVP(m_depth),
			CEREAL_NVP(m_texture_index),

			CEREAL_NVP(m_parent_index),
			CEREAL_NVP(m_child_index),
			CEREAL_NVP(m_sibling_index)
			);
	}

};

struct UIEvent
{
	uint32_t m_type;
	int32_t m_value[3];
};

struct UIBoundary
{
	uint32_t m_flag;
	float m_touch_time;
	uint32_t m_param_index;
	uint32_t _p;

	uint32_t m_touch_callback;
	uint32_t m_click_callback;
	uint32_t m_release_callback;
	uint32_t _p2;
};

struct UIWork 
{
	vec2 m_position;
	vec2 m_size;
	vec4 m_color;
};
struct UIAnimePlayInfo
{
	uint m_flag;
	int32_t m_anime_target;
	float m_frame;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(
			CEREAL_NVP(m_flag),
			CEREAL_NVP(m_anime_target),
			CEREAL_NVP(m_frame),

			);
	}
};

struct UIAnimeInfo
{
	uint m_target_frame;
	uint m_anime_num;
	uint m_anime_max_frame;

	UIAnimeInfo()
		: m_target_frame(60)
		, m_anime_max_frame(100)
	{

	}
	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(
			CEREAL_NVP(m_anime_num),
			CEREAL_NVP(m_anime_max_frame),
			CEREAL_NVP(m_target_frame),
			);
	}
};
struct UIAnimeKeyInfo
{
	enum type
	{
		type_pos_xy,
		type_size_xy,
		type_color_rgba,
		type_system_disable_order,
		type_max,
	};
	enum flag
	{
		is_enable = 1 << 0,
		is_erase = 1 << 1,
	};
	uint64_t m_target_hash;
	uint16_t m_target_index;
	uint16_t m_flag;
	uint16_t m_key_offset;	//!< オフセット
	uint8_t m_key_num;
	uint8_t m_type;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(
			CEREAL_NVP(m_target_hash),
			CEREAL_NVP(m_target_index),
			CEREAL_NVP(m_flag),
			);
	}
};
struct UIAnimeKeyData
{
	enum
	{
		is_enable = 1 << 0,
		is_erase = 1 << 1,
		interp_linear = 1 << 2,
		interp_switch = 1 << 3,
		interp_pow = 1<<4,
		interp_spline = 1 << 5,
		interp_bezier = 1 << 6,
	};
	uint32_t m_frame;
	uint32_t m_flag;
	union
	{
		int32_t		m_value_i;
		uint32_t	m_value_u;
		float		m_value_f;
		int16_t		m_value_i16[2];
		int8_t		m_value_i8[4];
		bool		m_value_b;
	};
	uint32_t _p;

	UIAnimeKeyData()
		: m_frame(0)
		, m_flag(0)
		, m_value_i(0)
	{}

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(
			CEREAL_NVP(m_frame),
			CEREAL_NVP(m_flag),
			CEREAL_NVP(m_value_i),
			);
	}

};


struct UIAnime
{
	btr::BufferMemoryEx<UIAnimeInfo> m_anime_info;
	btr::BufferMemoryEx<UIAnimeKeyInfo> m_anime_data_info;
	btr::BufferMemoryEx<UIAnimeKeyData> m_anime_key;

};

struct UI
{
	enum
	{
		USERID_MAX = 256,
		TEXTURE_MAX = 32,
		ANIME_NUM = 32,
	};
	std::string m_name;
	std::array<ResourceTexture, UI::TEXTURE_MAX> m_textures;

	vk::UniqueDescriptorSet m_descriptor_set;

	btr::BufferMemoryEx<vk::DrawIndirectCommand> m_draw_cmd;
	btr::BufferMemoryEx<UIInfo> m_info;
	btr::BufferMemoryEx<UIObject> m_object;
	btr::BufferMemoryEx<UIBoundary> m_boundary;
	btr::BufferMemoryEx<UIAnimePlayInfo> m_play_info;
	btr::BufferMemoryEx<uint32_t> m_user_id;
	btr::BufferMemoryEx<UIWork> m_tree;
	btr::BufferMemoryEx<UIEvent> m_event;
	btr::BufferMemoryEx<UIScene> m_scene;

	std::array<std::shared_ptr<UIAnime>, ANIME_NUM> m_anime_list;

	vk::UniqueImage m_ui_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_ui_texture_memory;

	std::shared_ptr<UI> m_parent;
	uint32_t m_parent_user_id;

	void setParent(const std::shared_ptr<UI>& parent, uint32_t parent_user_id)
	{
		m_parent = parent;
		m_parent_user_id = parent_user_id;
	}
	UI()
		: m_parent_user_id(0)
	{

	}
	~UI()
	{
		sDeleter::Order().enque(std::move(m_ui_image), std::move(m_image_view), std::move(m_ui_texture_memory));
	}
};
struct UIInstance
{
	std::shared_ptr<UI> m_ui;

	std::shared_ptr<UIInstance> m_parent;
	uint32_t m_parent_user_id;
	btr::BufferMemoryEx<UIWork> m_tree;

};
struct sUISystem : SingletonEx<sUISystem>
{
	friend SingletonEx<sUISystem>;

	sUISystem(const std::shared_ptr<btr::Context>& context);

	void addRender(std::shared_ptr<UI>& ui)
	{
		m_render[sGlobal::Order().getWorkerIndex()].push_back(ui);
	}
	vk::CommandBuffer draw();
private:
	enum Shader
	{
		SHADER_ANIMATION,
		SHADER_CLEAR,
		SHADER_UPDATE,
		SHADER_TRANSFORM,
		SHADER_BOUNDARY,
		SHADER_VERT_RENDER,
		SHADER_FRAG_RENDER,

		SHADER_NUM,
	};
	enum Pipeline
	{
		PIPELINE_ANIMATION,
		PIPELINE_CLEAR,
		PIPELINE_UPDATE,
		PIPELINE_TRANSFORM,
		PIPELINE_BOUNDARY,
		PIPELINE_RENDER,
		PIPELINE_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_ANIMATION,
		PIPELINE_LAYOUT_CLEAR,
		PIPELINE_LAYOUT_UPDATE,
		PIPELINE_LAYOUT_TRANSFORM,
		PIPELINE_LAYOUT_BOUNDARY,
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<RenderPassModule> m_render_pass;
	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout;
	vk::UniqueDescriptorPool m_descriptor_pool_anime;
	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout_anime;

	std::array<vk::UniqueShaderModule, SHADER_NUM>				m_shader_module;
	std::array<vk::UniquePipeline, PIPELINE_NUM>				m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;

	btr::BufferMemoryEx<UIGlobal> m_global;
	btr::BufferMemoryEx<UIWork> m_work;

	std::vector<std::shared_ptr<UI>> m_render[2];


};

