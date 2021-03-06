#pragma once

#include <vector>
#include <set>
#include <unordered_map>
#include <011_ui/sUISystem.h>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/rTexture.h>

#include <011_ui/cerealDefine.h>

struct UIEventFlag
{
	uint32_t m_flag;
};

struct UIEventAnime
{
	char m_target_name[32];
	char m_anime_name[32];
	uint32_t m_play_flag;
	uint32_t m_frame;
};
struct UIEventSE
{
	uint32_t m_flag;
	char m_se_name[32];
};

struct UIEvent
{
	uint32_t m_type;
	int32_t m_value[3];
};
struct UIEventTool
{
	char m_event_name[32];
	enum EventType
	{
		FlagOn,
		PlayAnime,
		PlaySe,
	};

	EventType m_event_type;
	union 
	{
		UIEventFlag m_flag;
		UIEventAnime m_anime;
		UIEventSE m_se;
	};
};

struct UIEventSetting
{
	char m_event_name[32];
};

struct Filename 
{
	char m_filename[32];
};
struct rUIScene
{
	std::vector<UIEventSetting> m_event_list;
	std::vector<Filename> m_filename;
};
struct UIObjectTool
{
	std::string m_object_name;
};

struct rUI 
{
	UIInfo m_info;
	std::vector<UIObject> m_object;
	std::array<std::string, UI::TEXTURE_MAX> m_texture_name;

	std::vector<UIObjectTool> m_object_tool;

	struct AnimeRequest
	{
		std::string m_object_name;
		std::string m_anime_name;
	};
	std::unordered_map<uint32_t, std::vector<AnimeRequest>> m_anime_list;

	struct BoundaryEvent
	{
		uint32_t m_object_index;
		uint32_t m_touch_type;
		UIEventTool m_event;
	};
	std::unordered_map<uint32_t, std::vector<BoundaryEvent>> m_boundary_list;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(m_info));
		archive(CEREAL_NVP(m_object));
		archive(CEREAL_NVP(m_texture_name));
//		archive(CEREAL_NVP(m_anime_play_info));
	}

	void reload(const std::shared_ptr<btr::Context>& context, std::shared_ptr<UI>& ui)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			btr::BufferMemoryDescriptorEx<vk::DrawIndirectCommand> desc;
			desc.element_num = 1;
			ui->m_draw_cmd = context->m_vertex_memory.allocateMemory(desc);

			cmd.updateBuffer<vk::DrawIndirectCommand>(ui->m_draw_cmd.getInfo().buffer, ui->m_draw_cmd.getInfo().offset, vk::DrawIndirectCommand(4, m_object.size(), 0, 0));
			{
				auto to_read = ui->m_draw_cmd.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_read }, {});
			}
		}
		{
			btr::BufferMemoryDescriptorEx<UIObject> desc;
			desc.element_num = m_object.size();
			ui->m_object = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), desc.element_num * sizeof(UIObject), m_object.data(), vector_sizeof(m_object));
			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(ui->m_object.getInfo().offset);
			copy.setSize(ui->m_object.getInfo().range);
			cmd.copyBuffer(staging.getInfo().buffer, ui->m_object.getInfo().buffer, copy);

			{
				auto to_read = ui->m_object.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_read }, {});
			}

		}
		{
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = 256;
			ui->m_user_id = context->m_storage_memory.allocateMemory(desc);

			cmd.fillBuffer(ui->m_user_id.getInfo().buffer, ui->m_user_id.getInfo().offset, ui->m_user_id.getInfo().range, 0u);
		}
		{
			btr::BufferMemoryDescriptorEx<UIWork> desc;
			desc.element_num = 256;
			ui->m_tree = context->m_storage_memory.allocateMemory(desc);

			cmd.fillBuffer(ui->m_tree.getInfo().buffer, ui->m_tree.getInfo().offset, ui->m_tree.getInfo().range, 0u);
		}
		{
			btr::BufferMemoryDescriptorEx<UIEvent> desc;
			desc.element_num = 256;
// 			ui->m_event = context->m_storage_memory.allocateMemory(desc);
// 			cmd.fillBuffer(ui->m_event.getInfo().buffer, ui->m_event.getInfo().offset, ui->m_event.getInfo().range, 0u);
		}
		{
			btr::BufferMemoryDescriptorEx<UIScene> desc;
			desc.element_num = 1;
			ui->m_scene = context->m_storage_memory.allocateMemory(desc);
			cmd.fillBuffer(ui->m_scene.getInfo().buffer, ui->m_scene.getInfo().offset, ui->m_scene.getInfo().range, 0u);
		}

		std::vector<UIBoundary> boundarys;
		{
			// 有効なバウンダリーをセット
			for (uint32_t i = 0; i < m_object.size(); i++)
			{
				auto& obj = m_object[i];
				if (btr::isOn(obj.m_flag, is_boundary)) {
					UIBoundary b{ i, 0.f, is_enable };
					boundarys.emplace_back(b);
				}
			}

			if (!boundarys.empty())
			{
				btr::BufferMemoryDescriptorEx<UIBoundary> desc;
				desc.element_num = boundarys.size();
				ui->m_boundary = context->m_storage_memory.allocateMemory(desc);

				desc.attribute |= btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), boundarys.data(), vector_sizeof(boundarys));

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(ui->m_boundary.getInfo().offset);
				copy.setSize(ui->m_boundary.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, ui->m_boundary.getInfo().buffer, copy);

				{
					auto to_read = ui->m_object.makeMemoryBarrier();
					to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
					to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_read }, {});
				}
			}
		}

		// useridの更新があった
		{
			btr::BufferMemoryDescriptorEx<uint32_t> desc;
			desc.element_num = UI::USERID_MAX;
			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memset(staging.getMappedPtr(), 0, UI::USERID_MAX * sizeof(uint32_t));
			for (uint32_t i = 0; i < m_object.size(); i++)
			{
				auto& p = m_object[i];
				if (p.m_user_id != 0)
				{
					*staging.getMappedPtr(p.m_user_id) = i;
				}
			}

			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(ui->m_user_id.getInfo().offset);
			copy.setSize(ui->m_user_id.getInfo().range);
			cmd.copyBuffer(staging.getInfo().buffer, ui->m_user_id.getInfo().buffer, copy);

		}

		int32_t max_depth = 0;
		for (auto& obj : m_object)
		{
			max_depth = glm::max<int32_t>(obj.m_depth, max_depth);
		}

		{
			m_info.m_depth_max = max_depth;
			m_info.m_object_num = m_object.size();
			m_info.m_boundary_num = boundarys.size();

			btr::BufferMemoryDescriptorEx<UIInfo> desc;
			desc.element_num = 1;
			ui->m_info = context->m_uniform_memory.allocateMemory(desc);

			cmd.updateBuffer<UIInfo>(ui->m_info.getInfo().buffer, ui->m_info.getInfo().offset, m_info);
			{
				auto to_read = ui->m_info.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_read }, {});
			}
		}

	}

	std::array<ResourceTexture, UI::TEXTURE_MAX> loadTexture(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd)
	{
		std::array<ResourceTexture, UI::TEXTURE_MAX> textures;
		for (size_t i = 0; i < m_texture_name.size(); i++)
		{
			if (!m_texture_name[i].empty())
			{
				textures[i].load(context, cmd, btr::getResourceAppPath() + "texture/" + m_texture_name[i]);
			}
		}
		return textures;
	}

	std::shared_ptr<UI> make(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		auto ui = std::make_shared<UI>();
		reload(context, ui);
		{
			btr::BufferMemoryDescriptorEx<UIAnimePlayInfo> desc;
			desc.element_num = 8;
			ui->m_play_info = context->m_storage_memory.allocateMemory(desc);
		}

		ui->m_textures = loadTexture(context, cmd);

		return ui;
	}
};

struct UIAnimeKey
{
	UIAnimeKeyInfo m_info;
	std::array<std::vector<UIAnimeKeyData>, UIAnimeKeyInfo::type_num> m_data;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(m_info));
		archive(CEREAL_NVP(m_data));
	}
};
struct rUIAnime
{
	std::string m_name;
	UIAnimeInfo m_info;
	std::vector<UIAnimeKey> m_key;

	template<class Archive>
	void serialize(Archive & archive)
	{
		archive(CEREAL_NVP(m_name));
		archive(CEREAL_NVP(m_info));
		archive(CEREAL_NVP(m_key));
	}

	std::shared_ptr<UIAnime> make(const std::shared_ptr<btr::Context>& context)const
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		uint32_t data_num = 0;
		uint32_t info_num = 0;
		{
			for (auto& key : m_key)
			{
				data_num += key.m_data.size();
				info_num++;
			}
			if (data_num == 0)
			{
				// データがないので不要
				return nullptr;
			}

// 			key_data.reserve(data_num);
// 			key_info.reserve(info_num);
// 			for (auto& key : m_key)
// 			{
// 				key_data.insert(key_data.end(), key.m_data.begin(), key.m_data.end());
// 				key_info.push_back(key.m_info);
// 			}
		}


		auto resource = std::make_shared<UIAnime>();
		{
			btr::BufferMemoryDescriptorEx<UIAnimeInfo> desc;
			desc.element_num = 1;
			resource->m_anime_info = context->m_uniform_memory.allocateMemory(desc);

			UIAnimeInfo info;
			info.m_anime_max_frame = m_info.m_anime_max_frame;
			info.m_anime_num = info_num;
			info.m_target_frame = m_info.m_target_frame;
			cmd.updateBuffer<UIAnimeInfo>(resource->m_anime_info.getInfo().buffer, resource->m_anime_info.getInfo().offset, info);

			{
				auto to_read = resource->m_anime_info.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_read }, {});
			}

		}
		{
			btr::BufferMemoryDescriptorEx<UIAnimeKeyInfo> desc;
			desc.element_num = info_num;
			resource->m_anime_data_info = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<UIAnimeKeyData> desc;
			desc.element_num = data_num;
			resource->m_anime_key = context->m_storage_memory.allocateMemory(desc);
		}
		auto desc_info = resource->m_anime_data_info.getDescriptor();
		desc_info.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_info = context->m_staging_memory.allocateMemory(desc_info);
		auto desc_key = resource->m_anime_key.getDescriptor();
		desc_key.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_key = context->m_staging_memory.allocateMemory(desc_key);

		uint32_t offset = 0;
		for (auto i = 0; i < m_key.size(); i++)
		{
			auto& key = m_key[i];
			staging_info.getMappedPtr(i)->m_target_hash = key.m_info.m_target_hash;
			staging_info.getMappedPtr(i)->m_target_index = key.m_info.m_target_index;
			staging_info.getMappedPtr(i)->m_flag = key.m_info.m_flag;
			staging_info.getMappedPtr(i)->m_type = key.m_info.m_type;
			staging_info.getMappedPtr(i)->m_key_num = key.m_data.size();
			staging_info.getMappedPtr(i)->m_key_offset = offset;

			for (auto& data : key.m_data)
			{
				for (auto& d : data)
				{
					*staging_key.getMappedPtr(offset) = d;
					offset++;
				}
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
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_write }, {});
		}
		{
			auto to_write = resource->m_anime_key.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_write }, {});
		}
		return resource;
	}
};
struct UIManipulater
{
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<UI> m_ui;
	rUI m_ui_resource;
	rUIAnime m_ui_anime_resource;


	UIManipulater(const std::shared_ptr<btr::Context>& context)
	{
		
		m_context = context;
		m_ui_resource.m_object.reserve(1024);
		UIObject root;
		root.m_position_local = vec2(320, 320);
		root.m_size_local = vec2(50, 50);
		root.m_color_local = vec4(1.f);
		root.m_depth = 0;
		root.m_parent_index = -1;
		root.m_sibling_index = -1;
		root.m_child_index = -1;
		root.m_flag = 0;
		root.m_flag |= is_visible;
		root.m_flag |= is_enable;
		root.m_user_id = 0;
		root.m_texture_index = 0;
		m_ui_resource.m_object.push_back(root);
		m_ui_resource.m_object_tool.emplace_back();

		// 初期設定
		UIAnimeKey key;
		key.m_info.m_flag = UIAnimeKeyInfo::is_enable;
		key.m_info.m_type = UIAnimeKeyInfo::type_pos_xy;
		key.m_info.m_target_index = 0;
		UIAnimeKeyData key_data;
		key_data.m_flag = UIAnimeKeyData::is_enable;
		key_data.m_frame = 0;
		key_data.m_value_i = 0;
		key.m_data[UIAnimeKeyInfo::type_pos_xy].push_back(key_data);
		m_ui_anime_resource.m_key.push_back(key);

		m_ui = m_ui_resource.make(context);
		auto anime = m_ui_anime_resource.make(context);
		m_ui->m_anime_list[0] = anime;

		m_ui->m_name = "New";
	}

	void animedataManip();

	void addnode(int32_t parent);

	struct Cmd
	{
		void undo() {}
		void redo() {}
	};
};
struct sUIManipulater : SingletonEx<sUIManipulater>
{
	friend SingletonEx<sUIManipulater>;

	std::vector<std::shared_ptr<UIManipulater>> m_manip_list;
	std::shared_ptr<UIManipulater> m_manip;
	std::vector<std::shared_ptr<rUIAnime>> m_anime_list;
	std::shared_ptr<rUIAnime> m_anime;
	std::vector<std::shared_ptr<rUIScene>> m_scene_list;
	std::shared_ptr<rUIScene> m_scene;
	int32_t m_object_index;
	int32_t m_anime_index;
	bool m_is_show_manip_window;
	bool m_is_show_tree_window;
	bool m_is_show_anime_window;
	bool m_is_show_texture_window;
	bool m_is_show_scene_window;
	bool m_request_update_boundary;
	bool m_request_update_sprite;
	bool m_request_update_animation;
	bool m_request_update_userid;
	bool m_request_update_texture;
	bool m_is_save;
	bool m_is_load;

	std::shared_ptr<btr::Context> m_context;
	sUIManipulater(const std::shared_ptr<btr::Context>& context)
		:m_object_index(-1)
		, m_anime_index(-1)
		, m_is_show_manip_window(false)
		, m_is_show_tree_window(false)
		, m_is_show_anime_window(false)
		, m_is_show_texture_window(false)
		, m_is_show_scene_window(false)
		, m_request_update_boundary(false)
		, m_request_update_sprite(false)
		, m_request_update_animation(false)
		, m_request_update_userid(false)
		, m_request_update_texture(false)
		, m_is_save(false)
		, m_is_load(false)
	{
		m_context = context;
		m_scene = std::make_shared<rUIScene>();
	}
	void execute(vk::CommandBuffer cmd);

	void manipWindow(std::shared_ptr<UIManipulater>& manip);

	void textureWindow(std::shared_ptr<UIManipulater>& manip);

	void treeWindow(std::shared_ptr<UIManipulater>& manip);
	void treeWindow(std::shared_ptr<UIManipulater>& manip, int32_t index);

	void animeWindow(std::shared_ptr<rUIAnime>& anime);
	void animedataManip(std::shared_ptr<rUIAnime>& anime);

	void sceneWindow(std::shared_ptr<rUIScene>& scene);
};