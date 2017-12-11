#pragma once

#include <vector>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>
struct UIGlobal
{
	uvec2 m_resolusion; // 解像度
};
struct UIInfo
{
	uint m_object_num; // node + sprite + boundary
	uint m_node_num;
	uint m_sprite_num;
	uint m_boundary_num;

	uint m_depth_max;
	uint _p11;
	uint _p12;
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
	uint32_t m_flag;
	int32_t m_parent_index;
	int32_t m_child_index;
	int32_t m_depth;

	int32_t m_chibiling_index;
	uint32_t _p11;
	uint32_t _p12;
	uint32_t _p13;

	vec2 m_position_anime; //!< animationで移動オフセット
	vec2 m_size_anime;
	vec4 m_color_anime;
	uint32_t m_flag_anime;
	uint32_t _p21;
	uint32_t _p22;
	uint32_t _p23;

	uint64_t m_name_hash;
	uint32_t _p32;
	uint32_t _p33;
};

struct UIBoundary
{
	uint32_t m_flag;
	float m_touch_time;
	uint m_param_index;
	uint _p;
};

struct UIWork 
{
	vec2 m_position;
	vec2 m_size;
	vec4 m_color;
};

struct UIAnimationInfo
{
	uint64_t m_target_hash;
	uint16_t m_target_index;
	uint16_t m_flag;
	uint16_t m_key_offset;	//!< オフセット
	uint16_t m_key_num;
};

struct UIAnimationKey
{
	enum
	{
		is_enable = 1 << 0,
		is_erase = 1 << 1,
	};
	uint32_t m_frame;
	uint32_t m_flag;
	union
	{
		int32_t		m_value_i;
		uint32_t	m_value_u;
		float		m_value_f;
		int16_t		m_value_i16[2];
	};
	uint32_t _p;

	UIAnimationKey()
		: m_frame(0)
		, m_flag(0)
		, m_value_i(0)
	{}
};


struct UIAnimationData
{
	enum
	{
		is_enable = 1 << 0,
		is_erase = 1 << 1,
		pos_xy = 1 << 2,
		size_xy = 1 << 3,
		color_rgba = 1 << 4,
	};

	uint64_t m_target_hash;
	uint16_t m_target_index;
	uint16_t m_flag;

	std::vector<UIAnimationKey> m_key;

	UIAnimationData()
		: m_target_hash(0)
		, m_target_index(0xffff)
		, m_flag(0)
	{}
};

struct UIAnimationResource
{
	btr::BufferMemoryEx<uint32_t> m_num;
	btr::BufferMemoryEx<UIAnimationInfo> m_anim_info;
	btr::BufferMemoryEx<UIAnimationKey> m_anim_key;
};
struct UI
{
	std::string m_name;

	btr::BufferMemoryEx<vk::DrawIndirectCommand> m_draw_cmd;
	btr::BufferMemoryEx<UIInfo> m_info;
	btr::BufferMemoryEx<UIParam> m_object;
	btr::BufferMemoryEx<UIBoundary> m_boundary;
	btr::BufferMemoryEx<UIWork> m_work;

	std::shared_ptr<UIAnimationResource> m_anime;
	vk::UniqueImage m_ui_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_ui_texture_memory;

//	std::array<Texture, 64> m_color_image;
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
	std::vector<UIAnimationData> m_data;

	UIAnimationData* getData(const std::string& name){
		std::hash<std::string> to_hash;
		auto hash = to_hash(name);
		for (auto& d : m_data) {
			if (d.m_target_hash == hash) {
				return &d;
			}
		}
		return nullptr;
	}

	std::shared_ptr<UIAnimationResource> makeResource(std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd)const
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

		std::shared_ptr<UIAnimationResource> resource = std::make_shared<UIAnimationResource>();
		{
			btr::BufferMemoryDescriptorEx<uint> desc;
			desc.element_num = 1;
			resource->m_num = context->m_uniform_memory.allocateMemory(desc);

			cmd.updateBuffer<uint32_t>(resource->m_num.getInfo().buffer, resource->m_num.getInfo().offset, m_data.size());
		}
		{
			btr::BufferMemoryDescriptorEx<UIAnimationInfo> desc;
			desc.element_num = m_data.size();
			resource->m_anim_info = context->m_storage_memory.allocateMemory(desc);
		}
		{
			uint32_t num = 0;
			for (auto& data : m_data)
			{
				num += data.m_key.size();
			}
			btr::BufferMemoryDescriptorEx<UIAnimationKey> desc;
			desc.element_num = num;
			resource->m_anim_key = context->m_storage_memory.allocateMemory(desc);
		}
		auto desc_info = resource->m_anim_info.getDescriptor();
		desc_info.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_info = context->m_staging_memory.allocateMemory(desc_info);
		auto desc_key = resource->m_anim_key.getDescriptor();
		desc_key.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_key = context->m_staging_memory.allocateMemory(desc_key);

		uint32_t offset = 0;
		for (auto i = 0; i < m_data.size(); i++)
		{
			auto& data = m_data[i];
			staging_info.getMappedPtr(i)->m_target_hash = data.m_target_hash;
			staging_info.getMappedPtr(i)->m_target_index = data.m_target_index;
			staging_info.getMappedPtr(i)->m_flag = data.m_flag;
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
		copy[0].setDstOffset(resource->m_anim_info.getInfo().offset);
		copy[0].setSize(staging_info.getInfo().range);
		copy[1].setSrcOffset(staging_key.getInfo().offset);
		copy[1].setDstOffset(resource->m_anim_key.getInfo().offset);
		copy[1].setSize(staging_key.getInfo().range);

		cmd.copyBuffer(staging_key.getInfo().buffer, resource->m_anim_key.getInfo().buffer, array_length(copy), copy);
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

struct UIManipulater 
{
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<UI> m_ui;
	std::vector<std::string> m_object_name;

	btr::BufferMemoryEx<UIInfo> m_info;
	btr::BufferMemoryEx<UIParam> m_object;
	btr::BufferMemoryEx<UIBoundary> m_boundary;
	std::vector<UiParamTool> m_object_tool;

	std::shared_ptr<UIAnimManipulater> m_anim_manip;

	btr::BufferMemoryEx<UIAnimationInfo> m_anim_info;
	btr::BufferMemoryEx<UIAnimationKey> m_anim_key;


	int32_t m_last_select_index;
	uint32_t m_object_counter;
	uint32_t m_boundary_counter;
	uint32_t m_sprite_counter;
	bool m_request_update_boundary;
	bool m_request_update_sprite;
	bool m_request_update_animation;
	UIManipulater(const std::shared_ptr<btr::Context>& context)
		: m_last_select_index(-1)
		, m_object_counter(0)
		, m_boundary_counter(0)
		, m_sprite_counter(0)
		, m_request_update_boundary(false)
		, m_request_update_sprite(false)
		, m_request_update_animation(false)
	{
		m_context = context;
		m_ui = std::make_shared<UI>();
		{
			btr::BufferMemoryDescriptorEx<UIInfo> desc;
			desc.element_num = 1;
			m_ui->m_info = context->m_uniform_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<vk::DrawIndirectCommand> desc;
			desc.element_num = 1;
			m_ui->m_draw_cmd = context->m_vertex_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<UIParam> desc;
			desc.element_num = 1024;
			m_ui->m_object = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<UIWork> desc;
			desc.element_num = m_ui->m_object.getDescriptor().element_num;
			m_ui->m_work = context->m_storage_memory.allocateMemory(desc);
		}

		{
			btr::BufferMemoryDescriptorEx<UIBoundary> desc;
			desc.element_num = m_ui->m_object.getDescriptor().element_num;
			m_ui->m_boundary = context->m_storage_memory.allocateMemory(desc);			
		}
		btr::BufferMemoryDescriptorEx<UIParam> desc;
		desc.element_num = 1024;
		m_object = context->m_staging_memory.allocateMemory(desc);
		btr::BufferMemoryDescriptorEx<UIBoundary> boundary_desc;
		boundary_desc.element_num = 1024;
		m_boundary = context->m_staging_memory.allocateMemory(boundary_desc);

		btr::BufferMemoryDescriptorEx<UIInfo> info_desc;
		info_desc.element_num = 1;
		m_info = context->m_staging_memory.allocateMemory(info_desc);

		UIParam root;
		root.m_position_local = vec2(320, 320);
		root.m_size_local = vec2(50, 50);
		root.m_color_local = vec4(1.f);
		root.m_depth = 0;
		root.m_parent_index = -1;
		root.m_chibiling_index = -1;
		root.m_child_index = -1;
		root.m_flag = 0;
		root.m_flag |= is_visible;
		root.m_flag |= is_enable;
		std::hash<std::string> hash;
		root.m_name_hash = hash(std::string("root"));
		*m_object.getMappedPtr(0) = root;
		m_object_counter++;

		m_object_tool.resize(1024);
		strcpy_s(m_object_tool[0].m_name.data(), m_object_tool[0].m_name.size(), "root");

		m_anim_manip = std::make_shared<UIAnimManipulater>();
		m_anim_manip->m_anime = std::make_shared<UIAnimation>();
	}

	vk::CommandBuffer execute();

	void dataManip();

	void drawtree(int32_t index);
	void animManip();
	void addnode(int32_t parent)
	{
		if (parent == -1)
		{
			return;
		}
		auto index = m_object_counter++;
		auto& parent_node = *m_object.getMappedPtr(parent);
		if (parent_node.m_child_index == -1) {
			parent_node.m_child_index = index;
		}
		else 
		{
			auto* n = m_object.getMappedPtr(parent_node.m_child_index);
			for (; n->m_chibiling_index != -1; n = m_object.getMappedPtr(n->m_chibiling_index))
			{}

			n->m_chibiling_index = index;
		}

		UIParam new_node;
		new_node.m_position_local;
		new_node.m_size_local = vec2(50, 50);
		new_node.m_depth = parent_node.m_depth+1;
		new_node.m_color_local = vec4(1.f);
		new_node.m_parent_index = parent;
		new_node.m_chibiling_index = -1;
		new_node.m_child_index = -1;
		new_node.m_flag = 0;
		new_node.m_flag |= is_visible;
		new_node.m_flag |= is_enable;

		{
			// 名前付け
			char buf[256] = {};
			sprintf_s(buf, "%s_%05d", m_object_tool[parent].m_name.data(), index);
			strcpy_s(m_object_tool[index].m_name.data(), m_object_tool[index].m_name.size(), buf);
			new_node.m_name_hash = m_object_tool[index].makeHash();
		}
		*m_object.getMappedPtr(index) = new_node;

	}

	struct Cmd 
	{
		void undo(){}
		void redo(){}
	};

};
struct sUISystem : SingletonEx<sUISystem>
{
	friend SingletonEx<sUISystem>;

	sUISystem(const std::shared_ptr<btr::Context>& context);
	std::shared_ptr<UI> create(const std::shared_ptr<btr::Context>& context);

	struct Scene 
	{
		std::array<std::shared_ptr<UIAnimationResource>, 32> m_animelist;
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

