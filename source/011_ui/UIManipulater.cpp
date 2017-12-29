#include <011_ui/UIManipulater.h>
#include <applib/sImGuiRenderer.h>
#include <applib/App.h>

#include <cereal/cereal.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>


vk::CommandBuffer UIManipulater::execute()
{
	{
		auto func = [this]()
		{
#if 1
			ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(200.f, 200.f), ImGuiCond_Once);
			if (ImGui::Begin(m_object_tool[0].m_name.data()))
			{
				if (ImGui::BeginPopupContextWindow("ui context"))
				{
					ImGui::MenuItem("Tree", NULL, &m_is_show_tree_window);
					ImGui::MenuItem("Manip", NULL, &m_is_show_manip_window);
					ImGui::MenuItem("Anime", NULL, &m_is_show_anime_window);
					ImGui::MenuItem("Texture", NULL, &m_is_show_texture_window);

					if (ImGui::MenuItem("NewWindow"))
					{
						{
							cWindowDescriptor window_info;
							window_info.class_name = L"Sub Window";
							window_info.window_name = L"Sub Window";
							window_info.backbuffer_num = sGlobal::Order().FRAME_MAX;
							window_info.size = vk::Extent2D(480, 480);
							window_info.surface_format_request = app::g_app_instance->m_window->getSwapchain().m_surface_format;
							auto sub = sWindow::Order().createWindow<AppWindow>(m_context, window_info);
							app::g_app_instance->pushWindow(sub);
						}
					}
					ImGui::EndPopup();
				}

				ImGui::End();
			}
#else
			ImGui::ShowTestWindow();
#endif
		};
		app::g_app_instance->m_window->pushImguiCmd(std::move(func));

	}

	if (m_is_show_manip_window)
	{
		auto func = [this]()
		{
			manipWindow();
		};
		app::g_app_instance->m_window->pushImguiCmd(std::move(func));
	}
	if (m_is_show_tree_window)
	{
		auto func = [this]()
		{
			treeWindow();
		};
		app::g_app_instance->m_window->pushImguiCmd(std::move(func));
	}

	if (m_is_show_anime_window)
	{
		auto func = [this]()
		{
			animeWindow();
		};
		app::g_app_instance->m_window->pushImguiCmd(std::move(func));
	}

	if (m_is_show_texture_window)
	{
		auto func = [this]()
		{
			textureWindow();
		};
		app::g_app_instance->m_window->pushImguiCmd(std::move(func));
	}

	auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);
	{
		{
			auto to_write = m_ui->m_draw_cmd.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_write }, {});
		}
		cmd.updateBuffer<vk::DrawIndirectCommand>(m_ui->m_draw_cmd.getInfo().buffer, m_ui->m_draw_cmd.getInfo().offset, vk::DrawIndirectCommand(4, m_object_counter, 0, 0));

		{
			auto to_read = m_ui->m_draw_cmd.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_read }, {});
		}

	}
	{
		{
			auto to_write = m_ui->m_object.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eVertexShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_write }, {});
		}
		vk::BufferCopy copy;
		copy.setSrcOffset(m_object.getInfo().offset);
		copy.setDstOffset(m_ui->m_object.getInfo().offset);
		copy.setSize(m_ui->m_object.getInfo().range);
		cmd.copyBuffer(m_object.getInfo().buffer, m_ui->m_object.getInfo().buffer, copy);

		{
			auto to_read = m_ui->m_object.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_read }, {});
		}
	}

	// boundaryの更新があった
	if (m_request_update_boundary)
	{
		m_request_update_boundary = false;
		m_boundary_counter = 0;
		btr::BufferMemoryDescriptorEx<UIBoundary> boundary_desc;
		boundary_desc.element_num = 1024;
		boundary_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = m_context->m_staging_memory.allocateMemory(boundary_desc);

		// 有効なバウンダリーをセット
		for (uint32_t i = 0; i < m_object_counter; i++)
		{
			auto* p = m_object.getMappedPtr(i);
			if (btr::isOn(p->m_flag, is_boundary)) {
				staging.getMappedPtr(m_boundary_counter)->m_param_index = i;
				staging.getMappedPtr(m_boundary_counter)->m_touch_time = 0.f;
				staging.getMappedPtr(m_boundary_counter)->m_flag = is_enable;
				m_boundary_counter++;
			}
		}

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_ui->m_boundary.getInfo().offset);
		copy.setSize(m_ui->m_boundary.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, m_ui->m_boundary.getInfo().buffer, copy);

	}

	// useridの更新があった
	if (m_request_update_userid)
	{
		m_request_update_userid = false;
		btr::BufferMemoryDescriptorEx<uint32_t> desc;
		desc.element_num = UI::USERID_MAX;
		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = m_context->m_staging_memory.allocateMemory(desc);

		memset(staging.getMappedPtr(), 0, UI::USERID_MAX * sizeof(uint32_t));
		for (uint32_t i = 0; i < m_object_counter; i++)
		{
			auto* p = m_object.getMappedPtr(i);
			if (p->m_user_id != 0)
			{
				*staging.getMappedPtr(p->m_user_id) = i;
			}
		}

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_ui->m_user_id.getInfo().offset);
		copy.setSize(m_ui->m_user_id.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, m_ui->m_user_id.getInfo().buffer, copy);

	}

	int32_t max_depth = 0;
	for (uint32_t i = 0; i < m_object_counter; i++)
	{
		max_depth = glm::max(m_object.getMappedPtr(i)->m_depth, max_depth);
	}

	m_info.getMappedPtr()->m_depth_max = max_depth;
	m_info.getMappedPtr()->m_object_num = m_object_counter;
	m_info.getMappedPtr()->m_boundary_num = m_boundary_counter;
	{
		{
			auto to_write = m_ui->m_info.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_write }, {});
		}
		vk::BufferCopy copy;
		copy.setSrcOffset(m_info.getInfo().offset);
		copy.setDstOffset(m_ui->m_info.getInfo().offset);
		copy.setSize(m_ui->m_info.getInfo().range);
		cmd.copyBuffer(m_info.getInfo().buffer, m_ui->m_info.getInfo().buffer, copy);

		{
			auto to_read = m_ui->m_info.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, { to_read }, {});
		}
	}

	if (m_request_update_animation)
	{
		m_request_update_animation = false;
		sDeleter::Order().enque(std::move(m_ui->m_anime));
		m_ui->m_anime = m_anim_manip->m_anime->makeResource(m_context, cmd);


		{
			auto to_write = m_ui->m_play_info.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			to_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_write }, {});
		}

		UIAnimePlayInfo info[8] = {};
		cmd.updateBuffer<UIAnimePlayInfo[8]>(m_ui->m_play_info.getInfo().buffer, m_ui->m_play_info.getInfo().offset, info);
		{
			auto to_write = m_ui->m_play_info.makeMemoryBarrier();
			to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_write.setDstAccessMask(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});
		}
	}
	if (m_request_update_texture)
	{
		for (size_t i = 0; i < m_texture_name.size(); i++)
		{
			if (m_texture_name[i][0] != '\0')
			{
				m_ui->m_textures[i] = ResourceTexture();
				m_ui->m_textures[i].load(m_context, cmd, btr::getResourceAppPath() + "texture/" + m_texture_name[i]);
			}
			else {
				m_ui->m_textures[i] = ResourceTexture();
			}
		}
	}

	sUISystem::Order().addRender(m_ui);

	cmd.end();
	return cmd;
}

void UIManipulater::manipWindow()
{
	ImGui::SetNextWindowSize(ImVec2(200.f, 500.f), ImGuiCond_Once);
	if (ImGui::Begin("Manip", &m_is_show_manip_window))
	{
		if (m_last_select_index >= 0)
		{
			auto* param = m_object.getMappedPtr(m_last_select_index);
			ImGui::CheckboxFlags("IsSprite", &param->m_flag, is_sprite);
			m_request_update_boundary = ImGui::CheckboxFlags("IsBoundary", &param->m_flag, is_boundary);
			ImGui::CheckboxFlags("IsErase", &param->m_flag, is_trash);
			ImGui::InputFloat2("Pos", &param->m_position_local[0]);
			ImGui::InputFloat2("Size", &param->m_size_local[0]);
			ImGui::ColorPicker4("Color", &param->m_color_local[0]);
			ImGui::InputText("name", m_object_tool[m_last_select_index].m_name.data(), m_object_tool[m_last_select_index].m_name.size(), 0);

			char buf[16] = {};
			sprintf_s(buf, "%d", param->m_user_id);
			ImGui::InputText("UserID", buf, 4, ImGuiInputTextFlags_CharsDecimal);
			param->m_user_id = atoi(buf);
			param->m_user_id = glm::clamp<int>(param->m_user_id, 0, UI::USERID_MAX - 1);

			sprintf_s(buf, "%d", param->m_texture_index);
			ImGui::InputText("TextureID", buf, 2, ImGuiInputTextFlags_CharsDecimal);
			param->m_texture_index = atoi(buf);
			param->m_texture_index = glm::clamp<int>(param->m_texture_index, 0, UI::TEXTURE_MAX - 1);
		}
		ImGui::End();
	}
}

void UIManipulater::textureWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400.f, 200.f), ImGuiCond_Once);
	if (ImGui::Begin("hoge", &m_is_show_texture_window, ImGuiWindowFlags_HorizontalScrollbar))
	{
		for (size_t i = 0; i < m_texture_name.size(); i++)
		{
			char label[16] = {};
			sprintf_s(label, "texture %2d", i);
			ImGui::InputText(label , m_texture_name[i], sizeof(m_texture_name[i]));
		}

		m_request_update_texture = ImGui::Button("texture_modifi");

		ImGui::End();
	}

}

void UIManipulater::treeWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400.f, 200.f), ImGuiCond_Once);
	if (ImGui::Begin("tree", &m_is_show_tree_window))
	{
		if (ImGui::Button("addchild"))
		{
			addnode(m_last_select_index);
		}
		treeWindow(0);
		ImGui::End();
	}
}
void UIManipulater::treeWindow(int32_t index)
{
	if (index == -1) { return; }
	bool is_open = ImGui::TreeNodeEx(m_object_tool[index].m_name.data(), m_last_select_index == index ? ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_OpenOnArrow : 0);
	if (ImGui::IsItemClicked(0)) {
		m_last_select_index = index;
	}
	if (is_open)
	{
		treeWindow(m_object.getMappedPtr(index)->m_child_index);
		ImGui::TreePop();
	}
	treeWindow(m_object.getMappedPtr(index)->m_chibiling_index);

}

void UIManipulater::animeWindow()
{
	// アニメーションのウインドウ
	ImGui::SetNextWindowPos(ImVec2(10.f, 220.f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(800.f, 200.f), ImGuiCond_Once);
	if (ImGui::Begin("anime", &m_is_show_anime_window))
	{
		if (ImGui::SmallButton(m_anim_manip->m_is_playing ? "STOP" : "PLAY")) {
			m_anim_manip->m_is_playing = !m_anim_manip->m_is_playing;
		}
		ImGui::SameLine();
		ImGui::DragInt("current frame", &m_anim_manip->m_frame, 0.1f, 0, 100);

		ImGui::Separator();

		if (m_last_select_index >= 0) {
			animedataManip();
		}

		ImGui::End();
	}

}

void UIManipulater::animedataManip()
{
	static int current_data_type;
	const char* types[] = { "pos", "size", "color", "disable order" };
	static_assert(array_length(types) == UIAnimeData::type_max, "");
	ImGui::Combo("anime data type", &current_data_type, types, array_length(types));
	UIAnimeData* data = m_anim_manip->m_anime->getData(m_object_tool[m_last_select_index].m_name.data(), (UIAnimeData::type)current_data_type);
	if (data)
	{
		auto& keys = data->m_key;

		if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
		{
			if (ImGui::BeginPopupContextWindow("key context"))
			{
				if (ImGui::Selectable("Add"))
				{
					UIAnimeKey new_key;
					new_key.m_flag = UIAnimeKey::is_enable;
					new_key.m_frame = m_anim_manip->m_frame;
					new_key.m_value_i = 0;
					keys.push_back(new_key);
				};
				if (ImGui::Selectable("Erase"))
				{
					for (auto it = keys.begin(); it != keys.end();)
					{
						if (btr::isOn(it->m_flag, UIAnimeKey::is_erase)) {
							it = keys.erase(it);
						}
						else {
							it++;
						}
					}
				};
				if (ImGui::Selectable("Sort"))
				{
					std::stable_sort(keys.begin(), keys.end(), [](auto&& a, auto&& b) { return a.m_frame < b.m_frame; });
				};
				if (ImGui::Selectable("Save"))
				{
					m_request_update_animation = true;
				};
				ImGui::EndPopup();
			}
			ImGui::Columns(4, "animcolumns");
			ImGui::Text("Frame"); ImGui::NextColumn();
			ImGui::Text("Value"); ImGui::NextColumn();
			ImGui::Text("Enable"); ImGui::NextColumn();
			ImGui::Text("Erase"); ImGui::NextColumn();
			ImGui::Separator();

			for (auto i = 0; i < keys.size(); i++)
			{
				char id[32] = {};
				sprintf_s(id, "key_%d", i);
				ImGui::PushID(id);

				auto& key = keys[i];

				sprintf_s(id, "frame_%d", i);
				ImGui::PushID(id);
				int32_t frame = key.m_frame;
				ImGui::DragInt("##frame", &frame, 0.1f, 0, 100);
				key.m_frame = frame;
				ImGui::NextColumn();
				ImGui::PopID();

				sprintf_s(id, "value_%d", i);
				ImGui::PushID(id);
				switch (current_data_type)
				{
				case UIAnimeData::type_pos_xy:
				case UIAnimeData::type_size_xy:
				{
					int value[2] = { key.m_value_i16[0], key.m_value_i16[1] };
					ImGui::DragInt2("##value", value, 0.1f);
					key.m_value_i16[0] = value[0];
					key.m_value_i16[1] = value[1];
					break;
				}
				case UIAnimeData::type_color_rgba:
				{
					int value[] = { key.m_value_i8[0], key.m_value_i8[1], key.m_value_i8[2], key.m_value_i8[3] };
					ImGui::DragInt4("##value", value, 0.1f, -127, 127);
					key.m_value_i8[0] = value[0];
					key.m_value_i8[1] = value[1];
					key.m_value_i8[2] = value[2];
					key.m_value_i8[3] = value[3];
					break;
				}
				case UIAnimeData::type_system_disable_order:
				{
					ImGui::Checkbox("##value", &key.m_value_b);
					break;
				}
				default:
					break;
				}
				ImGui::NextColumn();
				ImGui::PopID();

				sprintf_s(id, "enable_%d", i);
				ImGui::PushID(id);
				ImGui::CheckboxFlags("##enable", &key.m_flag, UIAnimeKey::is_enable);
				ImGui::NextColumn();
				ImGui::PopID();

				sprintf_s(id, "erase_%d", i);
				ImGui::PushID(id);
				ImGui::CheckboxFlags("##erase", &key.m_flag, UIAnimeKey::is_erase);
				ImGui::NextColumn();
				ImGui::PopID();

				ImGui::PopID();
				ImGui::Separator();
			}
			ImGui::Columns(1);
			ImGui::EndChild();
		}
	}
	else
	{
		if (ImGui::BeginPopupContextWindow("key context"))
		{
			if (ImGui::Selectable("Make Data"))
			{
				UIAnimeData data;
				data.m_target_index = m_last_select_index;
				data.m_target_hash = m_object_tool[m_last_select_index].makeHash();
				data.m_flag = 0;
				data.m_type = (UIAnimeData::type)current_data_type;
				m_anim_manip->m_anime->m_data.push_back(data);
			}
			ImGui::EndPopup();
		}

	}
}

