#pragma once

#include <vector>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>
struct UIGlobal
{
	uvec2 m_resolusion; // �𑜓x
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
	is_select = 1 << 30,	//!< �I��
	is_trash = 1 << 31,		//!< �j���ς�
};
struct UIParam
{
	vec2 m_position_local; //!< �����̏ꏊ
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

	vec2 m_position_anime; //!< animation�ňړ��I�t�Z�b�g
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
struct UIObject 
{
	std::string m_name;
	UIParam m_param;
};


struct UI
{
	std::string m_name;

	btr::BufferMemoryEx<vk::DrawIndirectCommand> m_draw_cmd;
	btr::BufferMemoryEx<UIInfo> m_info;
	btr::BufferMemoryEx<UIParam> m_object;
	btr::BufferMemoryEx<UIBoundary> m_boundary;
	btr::BufferMemoryEx<UIWork> m_work;

	vk::UniqueImage m_ui_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_ui_texture_memory;

//	std::array<Texture, 64> m_color_image;
};

// ���ԃo�b�t�@
struct UiParamTool
{
	std::string m_name;
	bool m_is_select;

	UiParamTool()
		: m_is_select(false)
	{}
};

struct UIAnimationInfo
{
	uint32_t m_target_index;
	uint32_t m_enable;
	uint32_t m_frame_num;
	uint32_t m_type;
};

struct UIAnimationKey
{
	int32_t m_frame;
	uint32_t m_flag;
	union 
	{
		float		m_value_f;
		uint32_t	m_value_u;
		int32_t		m_value_i;
	};
};
struct UIAnimationData
{
	enum 
	{
		is_enable = 1<<0,
	};
	uint32_t m_flag;
	std::vector<UIAnimationKey> m_key;
};

struct UIAnimationDataTool
{
	bool m_is_use;
};
struct UIAnimManipulater
{
	bool m_is_playing = false;
	int m_frame = 0;
	
	UIAnimationData m_pos_x;
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


	int32_t m_last_select_index;
	uint32_t m_object_counter;
	uint32_t m_boundary_counter;
	uint32_t m_sprite_counter;
	bool m_request_update_boundary;
	bool m_request_update_sprite;
	UIManipulater(const std::shared_ptr<btr::Context>& context)
		: m_last_select_index(-1)
		, m_object_counter(0)
		, m_boundary_counter(0)
		, m_sprite_counter(0)
		, m_request_update_boundary(false)
		, m_request_update_sprite(false)
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
		m_object_tool[0].m_name = "root";

		m_anim_manip = std::make_shared<UIAnimManipulater>();
	}
	void sort()
	{
	}

	vk::CommandBuffer execute();
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
		else {
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
			// ���O�t��
			char buf[256] = {};
			sprintf_s(buf, "%s_%05d", m_object_tool[parent].m_name.c_str(), index);
			m_object_tool[index].m_name = buf;
			std::hash<std::string> hash;
			new_node.m_name_hash = hash(std::string(m_object_tool[index].m_name));
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

	void addRender(std::shared_ptr<UI>& ui)
	{
		m_render.push_back(ui);
	}
	vk::CommandBuffer draw();
private:
	enum Shader
	{
		SHADER_ANIMATION,
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

	std::array<vk::UniqueShaderModule, SHADER_NUM>				m_shader_module;
	std::array<vk::UniquePipeline, PIPELINE_NUM>				m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;

	btr::BufferMemoryEx<UIGlobal> m_global;
	std::vector<std::shared_ptr<UI>> m_render;

};
