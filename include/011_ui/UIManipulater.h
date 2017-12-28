#pragma once
#include <011_ui/sUISystem.h>
#include <vector>
#include <btrlib/Context.h>
#include <btrlib/rTexture.h>

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

	btr::BufferMemoryEx<UIAnimeDataInfo> m_anim_info;
	btr::BufferMemoryEx<UIAnimeKey> m_anim_key;

	int32_t m_last_select_index;
	uint32_t m_object_counter;
	uint32_t m_boundary_counter;
	uint32_t m_sprite_counter;
	bool m_request_update_boundary;
	bool m_request_update_sprite;
	bool m_request_update_animation;
	bool m_request_update_userid;
	bool m_request_update_texture;
	bool m_is_show_anime_window;
	bool m_is_show_texture_window;

	std::array<char[64], UI::TEXTURE_MAX> m_texture_name;
	UIManipulater(const std::shared_ptr<btr::Context>& context)
		: m_last_select_index(-1)
		, m_object_counter(0)
		, m_boundary_counter(0)
		, m_sprite_counter(0)
		, m_request_update_boundary(false)
		, m_request_update_sprite(false)
		, m_request_update_animation(false)
		, m_request_update_userid(false)
		, m_request_update_texture(false)
		, m_is_show_anime_window(false)
		, m_is_show_texture_window(false)
	{
		for (auto& it : m_texture_name)
		{
			memset(it, 0, sizeof(it));
		}
		
		m_context = context;
		m_ui = std::make_shared<UI>();
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
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
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = 256;
			m_ui->m_user_id = context->m_storage_memory.allocateMemory(desc);

			cmd->fillBuffer(m_ui->m_user_id.getInfo().buffer, m_ui->m_user_id.getInfo().offset, m_ui->m_user_id.getInfo().range, 0u);
		}
		{
			btr::BufferMemoryDescriptorEx<UIEvent> desc;
			desc.element_num = 256;
			m_ui->m_event = context->m_storage_memory.allocateMemory(desc);
			cmd->fillBuffer(m_ui->m_event.getInfo().buffer, m_ui->m_event.getInfo().offset, m_ui->m_event.getInfo().range, 0u);
		}
		{
			btr::BufferMemoryDescriptorEx<UIScene> desc;
			desc.element_num = 1;
			m_ui->m_scene = context->m_storage_memory.allocateMemory(desc);
			cmd->fillBuffer(m_ui->m_scene.getInfo().buffer, m_ui->m_scene.getInfo().offset, m_ui->m_scene.getInfo().range, 0u);
		}

		{
			btr::BufferMemoryDescriptorEx<UIAnimePlayInfo> desc;
			desc.element_num = 8;
			m_ui->m_play_info = context->m_storage_memory.allocateMemory(desc);
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
		root.m_user_id = 0;
		root.m_texture_index = 0;
		*m_object.getMappedPtr(0) = root;
		m_object_counter++;

		m_object_tool.resize(1024);
		strcpy_s(m_object_tool[0].m_name.data(), m_object_tool[0].m_name.size(), "root");

		m_anim_manip = std::make_shared<UIAnimManipulater>();
		m_anim_manip->m_anime = std::make_shared<UIAnimation>();
		// ‰ŠúÝ’è
		UIAnimeData anime_data;
		anime_data.m_flag = UIAnimeData::is_enable;
		anime_data.m_type = UIAnimeData::type_pos_xy;
		anime_data.m_target_index = 0;
		anime_data.m_target_hash = m_object_tool[0].makeHash();
		UIAnimeKey key;
		key.m_flag = UIAnimeKey::is_enable;
		key.m_frame = 0;
		key.m_value_i = 0;
		anime_data.m_key.push_back(key);
		m_anim_manip->m_anime->m_data.push_back(anime_data);
		m_ui->m_anime = m_anim_manip->m_anime->makeResource(m_context, cmd.get());
	}

	vk::CommandBuffer execute();

	void textureWindow();

	void dataManip();

	void drawtree(int32_t index);
	void animeWindow();
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
			{
			}

			n->m_chibiling_index = index;
		}

		UIParam new_node;
		new_node.m_position_local;
		new_node.m_size_local = vec2(50, 50);
		new_node.m_depth = parent_node.m_depth + 1;
		new_node.m_color_local = vec4(1.f);
		new_node.m_parent_index = parent;
		new_node.m_chibiling_index = -1;
		new_node.m_child_index = -1;
		new_node.m_flag = 0;
		new_node.m_flag |= is_visible;
		new_node.m_flag |= is_enable;
		new_node.m_user_id = 0;
		{
			// –¼‘O•t‚¯
			char buf[256] = {};
			sprintf_s(buf, "%s_%05d", m_object_tool[parent].m_name.data(), index);
			strcpy_s(m_object_tool[index].m_name.data(), m_object_tool[index].m_name.size(), buf);
		}

		*m_object.getMappedPtr(index) = new_node;

	}

	struct Cmd
	{
		void undo() {}
		void redo() {}
	};

};
