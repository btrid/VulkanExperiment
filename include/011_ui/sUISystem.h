#pragma once

#include <vector>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>
#include <btrlib/rTexture.h>


struct UIGlobal
{
	uvec2 m_resolusion; // 解像度
};
struct UIScene
{
	uint32_t m_is_disable_order; // 操作無効中
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
struct UIParam
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
	int32_t m_chibiling_index;
	uint32_t _p23;

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
};

struct UIAnimeInfo
{
	uint m_anime_num;
	uint m_anime_frame;
	uint m_target_fps;
};
struct UIAnimeDataInfo
{
	uint64_t m_target_hash;
	uint16_t m_target_index;
	uint16_t m_flag;
	uint16_t m_key_offset;	//!< オフセット
	uint8_t m_key_num;
	uint8_t m_type;
};
struct UIAnimeKey
{
	enum
	{
		is_enable = 1 << 0,
		is_erase = 1 << 1,
//		interp_linear = 1 << 2,
		interp_switch = 1 << 3,
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

	UIAnimeKey()
		: m_frame(0)
		, m_flag(0)
		, m_value_i(0)
	{}
};


struct UIAnimeData
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
	uint16_t m_type;
	uint16_t m_flag;
	uint16_t _p;

	std::vector<UIAnimeKey> m_key;

	UIAnimeData()
		: m_target_hash(0)
		, m_target_index(0xffff)
		, m_type(0)
		, m_flag(0)
	{}
};


struct UIAnimeResource
{
	btr::BufferMemoryEx<UIAnimeInfo> m_anime_info;
	btr::BufferMemoryEx<UIAnimeDataInfo> m_anime_data_info;
	btr::BufferMemoryEx<UIAnimeKey> m_anime_key;
};

struct UIAnimeList
{
	enum 
	{
		LIST_NUM = 32,
	};
	std::array<std::shared_ptr<UIAnimeResource>, LIST_NUM> m_anime;
};

// 中間バッファ
struct UiParamTool
{
	std::array<char, 32> m_name;
	size_t makeHash()const 
	{
		std::hash<std::string> make_hash;
		return make_hash(m_name.data());
	}
	UiParamTool()
	{}
};




struct UIAnimation
{
	char m_name[32];
	char m_target_ui[32];
	std::vector<UIAnimeData> m_data;

	UIAnimeData* getData(const std::string& name, UIAnimeData::type type){
		std::hash<std::string> to_hash;
		auto hash = to_hash(name);
		for (auto& d : m_data) {
			if (d.m_target_hash == hash && d.m_type == type) {
				return &d;
			}
		}
		return nullptr;
	}

	std::shared_ptr<UIAnimeResource> makeResource(std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd)const
	{
		if (m_data.empty())
		{
			return nullptr;
		}
		{
			uint32_t num = 0;
			for (auto& data : m_data)
			{
				num += data.m_key.size();
			}
			
			if (num == 0)
			{
				// データがないので不要
				return nullptr;
			}
		}

		std::shared_ptr<UIAnimeResource> resource = std::make_shared<UIAnimeResource>();
		{
			btr::BufferMemoryDescriptorEx<UIAnimeInfo> desc;
			desc.element_num = 1;
			resource->m_anime_info = context->m_uniform_memory.allocateMemory(desc);

			UIAnimeInfo info;
			info.m_anime_frame = 100.f;
			info.m_anime_num = m_data.size();
			cmd.updateBuffer<UIAnimeInfo>(resource->m_anime_info.getInfo().buffer, resource->m_anime_info.getInfo().offset, info);
		}
		{
			btr::BufferMemoryDescriptorEx<UIAnimeDataInfo> desc;
			desc.element_num = m_data.size();
			resource->m_anime_data_info = context->m_storage_memory.allocateMemory(desc);

		}
		{
			uint32_t num = 0;
			for (auto& data : m_data)
			{
				num += data.m_key.size();
			}
			btr::BufferMemoryDescriptorEx<UIAnimeKey> desc;
			desc.element_num = num;
			resource->m_anime_key = context->m_storage_memory.allocateMemory(desc);
		}
		auto desc_info = resource->m_anime_data_info.getDescriptor();
		desc_info.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_info = context->m_staging_memory.allocateMemory(desc_info);
		auto desc_key = resource->m_anime_key.getDescriptor();
		desc_key.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_key = context->m_staging_memory.allocateMemory(desc_key);

		uint32_t offset = 0;
		for (auto i = 0; i < m_data.size(); i++)
		{
			auto& data = m_data[i];
			staging_info.getMappedPtr(i)->m_target_hash = data.m_target_hash;
			staging_info.getMappedPtr(i)->m_target_index = data.m_target_index;
			staging_info.getMappedPtr(i)->m_flag = data.m_flag;
			staging_info.getMappedPtr(i)->m_type = data.m_type;
			staging_info.getMappedPtr(i)->m_key_num = data.m_key.size();
			staging_info.getMappedPtr(i)->m_key_offset = offset;

			for (auto& key : data.m_key)
			{
				*staging_key.getMappedPtr(offset) = key;
				offset++;
			}
		}


		vk::BufferCopy copy[2];
		copy[0].setSrcOffset(staging_info.getInfo().offset);
		copy[0].setDstOffset(resource->m_anime_data_info.getInfo().offset);
		copy[0].setSize(staging_info.getInfo().range);
		copy[1].setSrcOffset(staging_key.getInfo().offset);
		copy[1].setDstOffset(resource->m_anime_key.getInfo().offset);
		copy[1].setSize(staging_key.getInfo().range);

		cmd.copyBuffer(staging_key.getInfo().buffer, resource->m_anime_key.getInfo().buffer, array_length(copy), copy);

		{
			auto to_write = resource->m_anime_data_info.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});
		}
		{
			auto to_write = resource->m_anime_key.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});
		}
		return resource;
	}
};


struct UIAnimationDataTool
{
	bool m_is_use;
};
struct UIAnimManipulater
{
	bool m_is_playing = false;
	int m_frame = 0;
	
	std::shared_ptr<UIAnimation> m_anime;
};

struct UI
{
	enum
	{
		USERID_MAX = 256,
		TEXTURE_MAX = 32,
	};
	std::string m_name;
//	std::array<char[64], TEXTURE_MAX> m_textures;
	std::array<ResourceTexture, UI::TEXTURE_MAX> m_textures;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> m_draw_cmd;
	btr::BufferMemoryEx<UIInfo> m_info;
	btr::BufferMemoryEx<UIParam> m_object;
	btr::BufferMemoryEx<UIBoundary> m_boundary;
	btr::BufferMemoryEx<UIWork> m_work;
	btr::BufferMemoryEx<UIAnimePlayInfo> m_play_info;
	btr::BufferMemoryEx<uint32_t> m_user_id;
	btr::BufferMemoryEx<UIEvent> m_event;
	btr::BufferMemoryEx<UIScene> m_scene;
	std::shared_ptr<UIAnimeResource> m_anime;
	std::shared_ptr<UIAnimeList> m_anime_list;
	vk::UniqueImage m_ui_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_ui_texture_memory;

	template <class Archive>
	void serialize(Archive & ar)
	{
		ar(CEREAL_NVP(m_name));
		m_info.getMappedPtr();
	}

	//	std::array<Texture, 64> m_color_image;
};

struct sUISystem : SingletonEx<sUISystem>
{
	friend SingletonEx<sUISystem>;

	sUISystem(const std::shared_ptr<btr::Context>& context);
	std::shared_ptr<UI> create(const std::shared_ptr<btr::Context>& context);

	struct Scene 
	{
		std::array<std::shared_ptr<UIAnimeResource>, 32> m_animelist;
	};
	void addRender(std::shared_ptr<UI>& ui)
	{
		m_render.push_back(ui);
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
	std::vector<std::shared_ptr<UI>> m_render;

};

