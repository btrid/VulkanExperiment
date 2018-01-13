#include <011_ui/UIManipulater.h>
#include <applib/sImGuiRenderer.h>
#include <applib/App.h>


void UIManipulater::execute()
{
	{
		auto func = [this]()
		{
#if 1
			ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(200.f, 200.f), ImGuiCond_Once);
			if (ImGui::Begin("jhgjhghj"))
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
							window_info.window_name = L"Sub Window";
							window_info.backbuffer_num = sGlobal::Order().FRAME_MAX;
							window_info.size = vk::Extent2D(480, 480);
							window_info.surface_format_request = app::g_app_instance->m_window->getSwapchain().m_surface_format;
							app::g_app_instance->pushWindow(window_info);
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
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));

	}

	if (m_is_show_manip_window)
	{
		auto func = [this]()
		{
			manipWindow();
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}
	if (m_is_show_tree_window)
	{
		auto func = [this]()
		{
			treeWindow();
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}

	if (m_is_show_anime_window)
	{
		auto func = [this]()
		{
			animeWindow();
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}

	if (m_is_show_texture_window)
	{
		auto func = [this]()
		{
			textureWindow();
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}

	sUISystem::Order().addRender(m_ui);
}

void UIManipulater::manipWindow()
{
	ImGui::SetNextWindowSize(ImVec2(200.f, 500.f), ImGuiCond_Once);
	if (ImGui::Begin("Manip", &m_is_show_manip_window))
	{
		if (m_last_select_index >= 0)
		{
			auto& obj = m_ui_resource.m_object[m_last_select_index];
			ImGui::CheckboxFlags("IsSprite", &obj.m_flag, is_sprite);
			m_request_update_boundary = ImGui::CheckboxFlags("IsBoundary", &obj.m_flag, is_boundary);
			ImGui::CheckboxFlags("IsErase", &obj.m_flag, is_trash);
			ImGui::InputFloat2("Pos", &obj.m_position_local[0]);
			ImGui::InputFloat2("Size", &obj.m_size_local[0]);
			ImGui::ColorPicker4("Color", &obj.m_color_local[0]);
//			ImGui::InputText("name", m_object_tool[m_last_select_index].m_name.data(), m_object_tool[m_last_select_index].m_name.size(), 0);

			char buf[16] = {};
			sprintf_s(buf, "%d", obj.m_user_id);
			ImGui::InputText("UserID", buf, 4, ImGuiInputTextFlags_CharsDecimal);
			obj.m_user_id = atoi(buf);
			obj.m_user_id = glm::clamp<int>(obj.m_user_id, 0, UI::USERID_MAX - 1);

			sprintf_s(buf, "%d", obj.m_texture_index);
			ImGui::InputText("TextureID", buf, 2, ImGuiInputTextFlags_CharsDecimal);
			obj.m_texture_index = atoi(buf);
			obj.m_texture_index = glm::clamp<int>(obj.m_texture_index, 0, UI::TEXTURE_MAX - 1);
		}
		ImGui::End();
	}
}

void UIManipulater::textureWindow()
{
	ImGui::SetNextWindowSize(ImVec2(400.f, 200.f), ImGuiCond_Once);
	if (ImGui::Begin("hoge", &m_is_show_texture_window, ImGuiWindowFlags_HorizontalScrollbar))
	{
		for (uint32_t i = 0; i < m_ui_resource.m_texture_name.size(); i++)
		{
			char label[16] = {};
			sprintf_s(label, "texture %2d", i);

//			ImGui::InputText(label, m_texture_name[i], sizeof(m_texture_name[i]));

			char buf[64] = {};
			sprintf_s(buf, m_ui_resource.m_texture_name[i].c_str());
			ImGui::InputText(label, buf, sizeof(buf)-1);
			m_ui_resource.m_texture_name[i] = buf;
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
	bool is_open = ImGui::TreeNodeEx("objtree", m_last_select_index == index ? ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_OpenOnArrow : 0, "%d", index );
	if (ImGui::IsItemClicked(0)) {
		m_last_select_index = index;
	}
	if (is_open)
	{
		treeWindow(m_ui_resource.m_object[index].m_child_index);
		ImGui::TreePop();
	}
	treeWindow(m_ui_resource.m_object[index].m_sibling_index);

}

void UIManipulater::animeWindow()
{
	// アニメーションのウインドウ
	ImGui::SetNextWindowPos(ImVec2(10.f, 220.f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(800.f, 200.f), ImGuiCond_Once);
	if (ImGui::Begin("anime", &m_is_show_anime_window))
	{
// 		if (ImGui::SmallButton(m_anim_manip->m_is_playing ? "STOP" : "PLAY")) {
// 			m_anim_manip->m_is_playing = !m_anim_manip->m_is_playing;
// 		}
//		ImGui::SameLine();
//		ImGui::DragInt("current frame", &m_anim_manip->m_frame, 0.1f, 0, 100);

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
	static_assert(array_length(types) == UIAnimeKeyInfo::type_max, "");
	ImGui::Combo("anime data type", &current_data_type, types, array_length(types));
	std::vector<UIAnimeKeyData> keys;
//	if (keys)
	{
//		auto& keys = data->m_key;

		if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar))
		{
			if (ImGui::BeginPopupContextWindow("key context"))
			{
				if (ImGui::Selectable("Add"))
				{
					UIAnimeKeyData new_key;
					new_key.m_flag = UIAnimeKeyData::is_enable;
					new_key.m_frame = 0;
					new_key.m_value_i = 0;
					keys.push_back(new_key);
				};
				if (ImGui::Selectable("Erase"))
				{
					for (auto it = keys.begin(); it != keys.end();)
					{
						if (btr::isOn(it->m_flag, UIAnimeKeyData::is_erase)) {
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
				case UIAnimeKeyInfo::type_pos_xy:
				case UIAnimeKeyInfo::type_size_xy:
				{
					int value[2] = { key.m_value_i16[0], key.m_value_i16[1] };
					ImGui::DragInt2("##value", value, 0.1f);
					key.m_value_i16[0] = value[0];
					key.m_value_i16[1] = value[1];
					break;
				}
				case UIAnimeKeyInfo::type_color_rgba:
				{
					int value[] = { key.m_value_i8[0], key.m_value_i8[1], key.m_value_i8[2], key.m_value_i8[3] };
					ImGui::DragInt4("##value", value, 0.1f, -127, 127);
					key.m_value_i8[0] = value[0];
					key.m_value_i8[1] = value[1];
					key.m_value_i8[2] = value[2];
					key.m_value_i8[3] = value[3];
					break;
				}
				case UIAnimeKeyInfo::type_system_disable_order:
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
				ImGui::CheckboxFlags("##enable", &key.m_flag, UIAnimeKeyData::is_enable);
				ImGui::NextColumn();
				ImGui::PopID();

				sprintf_s(id, "erase_%d", i);
				ImGui::PushID(id);
				ImGui::CheckboxFlags("##erase", &key.m_flag, UIAnimeKeyData::is_erase);
				ImGui::NextColumn();
				ImGui::PopID();

				ImGui::PopID();
				ImGui::Separator();
			}
			ImGui::Columns(1);
			ImGui::EndChild();
		}
	}
// 	else
// 	{
// 		if (ImGui::BeginPopupContextWindow("key context"))
// 		{
// 			if (ImGui::Selectable("Make Data"))
// 			{
// 				UIAnimeData data;
// 				data.m_target_index = m_last_select_index;
// 				data.m_target_hash = m_object_tool[m_last_select_index].makeHash();
// 				data.m_flag = 0;
// 				data.m_type = (UIAnimeData::type)current_data_type;
// 				m_anim_manip->m_anime->m_data.push_back(data);
// 			}
// 			ImGui::EndPopup();
// 		}
// 
// 	}
}

void UIManipulater::addnode(int32_t parent)
{
	if (parent == -1)
	{
		return;
	}
//	auto index = m_object_counter++;
	auto& parent_node = m_ui_resource.m_object[parent];
	if (parent_node.m_child_index == -1) {
		parent_node.m_child_index = m_ui_resource.m_object.size();
	}
	else
	{
		auto& child_node = m_ui_resource.m_object[parent_node.m_child_index];
		for (; child_node.m_sibling_index != -1; child_node = m_ui_resource.m_object[child_node.m_sibling_index])
		{
		}

		child_node.m_sibling_index = m_ui_resource.m_object.size();
	}

	UIObject new_node;
	new_node.m_position_local;
	new_node.m_size_local = vec2(50, 50);
	new_node.m_depth = parent_node.m_depth + 1;
	new_node.m_color_local = vec4(1.f);
	new_node.m_parent_index = parent;
	new_node.m_sibling_index = -1;
	new_node.m_child_index = -1;
	new_node.m_flag = 0;
	new_node.m_flag |= is_visible;
	new_node.m_flag |= is_enable;
	new_node.m_user_id = 0;
	{
		// 名前付け
		char buf[256] = {};
//		sprintf_s(buf, "%s_%05d", m_object_tool[parent].m_name.data(), index);
//		strcpy_s(m_object_tool[index].m_name.data(), m_object_tool[index].m_name.size(), buf);
	}

	m_ui_resource.m_object.push_back(new_node);
}


