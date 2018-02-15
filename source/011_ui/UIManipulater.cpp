#include <011_ui/UIManipulater.h>
#include <filesystem>

#include <applib/sImGuiRenderer.h>
#include <applib/App.h>
#include <011_ui/Font.h>

struct NodeSelectModal
{
	enum Event
	{
		CONTINUE,
		DICIDE,
		CANCEL,
	};

	int32_t m_select;
	NodeSelectModal()
	{
		m_select = -1;
		ImGui::OpenPopup("NodeSelect");
	}

	~NodeSelectModal()
	{
		ImGui::CloseCurrentPopup();
	}

	void updateImpl(std::shared_ptr<UIManipulater>& manip, int32_t index)
	{
		if (index == -1) { return; }
		ImVec4 color = manip->m_ui_resource.m_object[index].m_user_id != 0 ? ImVec4(1.f, 0.7f, 0.7f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f);
		ImGui::PushStyleColor(ImGuiCol_Text, color);

		char name[256];
		sprintf_s(name, "%s : %d", manip->m_ui_resource.m_object_tool[index].m_object_name.c_str(), index);

		bool is_open = ImGui::TreeNodeEx("objtree", m_select == index ? ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_OpenOnArrow : 0, name);
		ImGui::PopStyleColor();
		if (ImGui::IsItemClicked(0)) {
			m_select = index;
		}
		if (is_open)
		{
			updateImpl(manip, manip->m_ui_resource.m_object[index].m_child_index);
			ImGui::TreePop();
		}
		updateImpl(manip, manip->m_ui_resource.m_object[index].m_sibling_index);
	}

	Event update(std::shared_ptr<UIManipulater>& manip)
	{
		ImGui::SetNextWindowSize(ImVec2(400.f, 200), ImGuiCond_Once);
		if (ImGui::BeginPopupModal("NodeSelect"))
		{
//			if (ImGui::BeginChild("nodeselectchild", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar) )
			{
				updateImpl(manip, 0);
//				ImGui::EndChild();
			}

			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				ImGui::EndPopup();
				return CANCEL;
			}

			ImGui::EndPopup();
			return CONTINUE;
		}
		return CONTINUE;
	}

	int32_t get()const
	{
		return m_select;
	}


};
struct FileSelectModal 
{
	enum Event
	{
		CONTINUE,
		DICIDE,
		CANCEL,
	};
	ImGuiTextFilter m_filter;
	std::experimental::filesystem::path m_dir;
	std::string m_value;
	FileSelectModal()
	{
		m_dir = std::experimental::filesystem::current_path().parent_path();
		ImGui::OpenPopup("FileSelect");
	}

	~FileSelectModal()
	{
		ImGui::CloseCurrentPopup();
	}
	Event update()
	{
		if (ImGui::BeginPopupModal("FileSelect", NULL, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("%s", m_dir.generic_string().c_str());
			m_filter.Draw();
			if (ImGui::Selectable("../", false, ImGuiSelectableFlags_DontClosePopups))
			{
				m_dir = std::experimental::filesystem::system_complete(m_dir.parent_path());
			}
			else
			{
				auto file_it = std::experimental::filesystem::directory_iterator(m_dir);
				for (auto file = std::experimental::filesystem::begin(file_it); file != std::experimental::filesystem::end(file_it); file++)
				{
					auto path = file->path().generic_string();
					auto name = file->path().filename().generic_string();
					if (m_filter.PassFilter(name.c_str()))
					{
						auto is_directory = std::experimental::filesystem::is_directory(path.c_str());
						auto is_file = std::experimental::filesystem::is_regular_file(path.c_str());
						if (is_directory)
						{
							name += "/";
						}
						if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_DontClosePopups))
						{
							if (is_directory)
							{
								m_dir = m_dir.generic_string() + "/" + file->path().filename().generic_string();
								break;
							}
							else if (is_file)
							{
								m_value = name;
								ImGui::EndPopup();
								return DICIDE;
							}
						}
					}
				}
			}
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				ImGui::EndPopup();
				return CANCEL;
			}

		}

		ImGui::EndPopup();
		return CONTINUE;
	}

	std::string get()const
	{
		return m_value;
	}
};
void UIManipulater::addnode(int32_t parent)
{
	if (parent == -1)
	{
		return;
	}

	auto& parent_node = m_ui_resource.m_object[parent];
	if (parent_node.m_child_index == -1) {
		parent_node.m_child_index = m_ui_resource.m_object.size();
	}
	else
	{
		auto* child_node = &m_ui_resource.m_object[parent_node.m_child_index];
		for (; child_node->m_sibling_index != -1; child_node = &m_ui_resource.m_object[child_node->m_sibling_index])
		{
		}

		child_node->m_sibling_index = m_ui_resource.m_object.size();
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
	m_ui_resource.m_object_tool.emplace_back();
}


void sUIManipulater::execute(vk::CommandBuffer cmd)
{
// 	auto ui_resource = m_ui_resource;
// 	auto ui_anime_resource = m_ui_anime_resource;


	// 	if (m_is_save)
	// 	{
	// 		m_is_save = false;
	// 		std::ofstream os(btr::getResourceAppPath() + "ui\\" + "data.json");
	// 		{
	// 			cereal::JSONOutputArchive o_archive(os);
	// 			o_archive(ui_resource);
	// 		}
	// 	}
	// 	ui_resource.reload(m_context, m_ui);
	// 	if (m_request_update_texture) 
	// 	{
	// 		m_request_update_texture = false;
	// 		auto cmd = m_context->m_cmd_pool->allocCmdTempolary(0);
	// 		m_ui->m_textures = ui_resource.loadTexture(m_context, cmd);
	// 	}
	// 	m_ui->m_anime_list[0] = m_ui_anime_resource.make(m_context);

	{
		auto func = [this]()
		{
#if 1
			ImGui::SetNextWindowPos(ImVec2(10.f, 10.f), ImGuiCond_Once);
			ImGui::SetNextWindowSize(ImVec2(200.f, 200.f), ImGuiCond_Once);
			if (ImGui::Begin("ui manipulater"))
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

					if (ImGui::MenuItem("New"))
					{
						m_manip_list.emplace_back(std::make_shared<UIManipulater>(m_context));
					}
					if (ImGui::MenuItem("New Anime"))
					{
						auto anime = std::make_shared<rUIAnime>();
						anime->m_name = "New Anime";
						m_anime_list.emplace_back(anime);
					}

//					ImGui::MenuItem("Save", NULL, &m_is_save);
// 					if(ImGui::MenuItem("Load"))
// 					{
// 						m_is_load = false;
// 						std::shared_ptr<UIManipulater> manip;
// 						std::ifstream is(btr::getResourceAppPath() + "ui\\" + "data.json");
// 						{
// 							cereal::JSONInputArchive i_archive(is);
// 							i_archive(m_ui_resource);
// 							m_request_update_texture = true;
// 						}
// 
// 					}
					ImGui::EndPopup();
				}

				for (auto& manip : m_manip_list)
				{
					bool selected = false;
					ImGui::Selectable(manip->m_ui->m_name.c_str(), &selected);
					if (selected)
					{
						m_manip = manip;
					}
				}
				ImGui::Separator();
				for (auto& anime : m_anime_list)
				{
					bool selected = false;
					ImGui::Selectable(anime->m_name.c_str(), &selected);
					if (selected)
					{
						m_anime = anime;
					}
				}
				ImGui::End();
			}
	// 		if (m_is_load)
	// 		{
	// 			m_is_load = false;
	// 			std::ifstream is(btr::getResourceAppPath() + "ui\\" + "data.json");
	// 			{
	// 				cereal::JSONInputArchive i_archive(is);
	// 				i_archive(m_ui_resource);
	// 				m_request_update_texture = true;
	// 			}
	// 		}
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
			manipWindow(m_manip);
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}
	if (m_is_show_tree_window)
	{
		auto func = [this]()
		{
			treeWindow(m_manip);
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}

	if (m_is_show_anime_window)
	{
		auto func = [this]()
		{
			animeWindow(m_anime);
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}

	if (m_manip && m_is_show_texture_window)
	{
		auto func = [this]()
		{
			textureWindow(m_manip);
		};
		app::g_app_instance->m_window->getImguiPipeline()->pushImguiCmd(std::move(func));
	}

	for (auto& manip : m_manip_list)
	{
		sUISystem::Order().addRender(manip->m_ui);
	}

}

void sUIManipulater::manipWindow(std::shared_ptr<UIManipulater>& manip)
{
	ImGui::SetNextWindowSize(ImVec2(200.f, 500.f), ImGuiCond_Once);
	if (ImGui::Begin("Manip", &m_is_show_manip_window))
	{
		if (m_manip && m_object_index >= 0)
		{
			auto& obj = manip->m_ui_resource.m_object[m_object_index];
			ImGui::CheckboxFlags("IsSprite", &obj.m_flag, is_sprite);
			m_request_update_boundary = ImGui::CheckboxFlags("IsBoundary", &obj.m_flag, is_boundary);
			ImGui::CheckboxFlags("IsErase", &obj.m_flag, is_trash);
			ImGui::InputFloat2("Pos", &obj.m_position_local[0]);
			ImGui::InputFloat2("Size", &obj.m_size_local[0]);
			ImGui::ColorPicker4("Color", &obj.m_color_local[0]);

			char buf[16] = {};
			sprintf_s(buf, "%d", obj.m_user_id);
			ImGui::InputText("UserID", buf, 4, ImGuiInputTextFlags_CharsDecimal);
			obj.m_user_id = atoi(buf);
			obj.m_user_id = glm::clamp<int>(obj.m_user_id, 0, UI::USERID_MAX - 1);

			sprintf_s(buf, "%d", obj.m_texture_index);
			ImGui::InputText("TextureID", buf, 2, ImGuiInputTextFlags_CharsDecimal);
			obj.m_texture_index = atoi(buf);
			obj.m_texture_index = glm::clamp<int>(obj.m_texture_index, 0, UI::TEXTURE_MAX - 1);

			{
				// boundary
				auto it = manip->m_ui_resource.m_boundary_list.find(m_object_index);
				if (it != manip->m_ui_resource.m_boundary_list.end())
				{
					for (auto& boundary_event : it->second)
					{
						ImGui::PushID(&boundary_event);
						{
							int touchtype = boundary_event.m_touch_type;
							const char* touchtype_msg[] = { "on", "off", "in", "out", "hover" };
							ImGui::ListBox("touch type", &touchtype, touchtype_msg, array_length(touchtype_msg));
							boundary_event.m_touch_type = touchtype;
						}
						{
							int eventtype = boundary_event.m_event_type;
							const char* eventtype_msg[] = { "end", "play anime", };
							ImGui::ListBox("event type", &eventtype, eventtype_msg, array_length(eventtype_msg));
							boundary_event.m_event_type = eventtype;

							switch (eventtype)
							{
							case rUI::BoundaryEvent::event_end:
							{
							}
							break;
							case rUI::BoundaryEvent::event_play_anime:
							{

							}
							break;
							default:
								break;
							}
						}
						ImGui::PopID();
					}
				}
				static std::unique_ptr<NodeSelectModal> modal;
				if (ImGui::Button("Boundary Event"))
				{
					modal = std::make_unique<NodeSelectModal>();
				}
				if (modal)
				{
					auto ev = modal->update(m_manip);
					if (ev == NodeSelectModal::DICIDE)
					{
// 							auto it = manip->m_ui_resource.m_anime_list.find(m_object_index);
// 							if (it == manip->m_ui_resource.m_anime_list.end())
// 							{
// 								manip->m_ui_resource.m_anime_list[m_object_index] = std::vector<rUI::AnimeRequest>();
// 								it = manip->m_ui_resource.m_anime_list.find(m_object_index);
// 							}
// 
// 							rUI::AnimeRequest request;
// 							//									request.m_object_index = m_object_index;
// 							request.m_anime_name = modal->get();
// 							manip->m_ui_resource.m_anime_list[m_object_index].emplace_back(request);
						modal.reset();
					}
					else if (ev == FileSelectModal::CANCEL)
					{
						modal.reset();
					}
				}
			}

			{
				// anime
				auto it = manip->m_ui_resource.m_anime_list.find(m_object_index);
				if (it != manip->m_ui_resource.m_anime_list.end())
				{
					for (auto request = it->second.begin(); request != it->second.end();)
					{
					ImGui::Text("%s", request->m_anime_name.c_str());
						if (ImGui::Button("Erase", ImVec2(120, 0))) {
							request = it->second.erase(request);
						}
						else {
							request++;
						}
					}

					if (it->second.empty()) {
						manip->m_ui_resource.m_anime_list.erase(it);
					}
				}
			}

			static std::unique_ptr<FileSelectModal> modal;
			if (ImGui::Button("Add Anime Slot")) {
				modal = std::make_unique<FileSelectModal>();
			}
			if (modal)
			{
				auto ev = modal->update();
				if (ev == FileSelectModal::DICIDE)
				{
					auto it = manip->m_ui_resource.m_anime_list.find(m_object_index);
					if (it == manip->m_ui_resource.m_anime_list.end())
					{
						manip->m_ui_resource.m_anime_list[m_object_index] = std::vector<rUI::AnimeRequest>();
						it = manip->m_ui_resource.m_anime_list.find(m_object_index);
					}

					rUI::AnimeRequest request;
					//									request.m_object_index = m_object_index;
					request.m_anime_name = modal->get();
					manip->m_ui_resource.m_anime_list[m_object_index].emplace_back(request);
					modal.reset();
				}
				else if (ev == FileSelectModal::CANCEL)
				{
					modal.reset();
				}
			}

// 			ImGui::EndChild();
// 			ImGui::EndGroup();

		}
		ImGui::End();
	}
}

void sUIManipulater::textureWindow(std::shared_ptr<UIManipulater>& manip)
{
	ImGui::SetNextWindowSize(ImVec2(400.f, 200.f), ImGuiCond_Once);
	if (ImGui::Begin("hoge", &m_is_show_texture_window, ImGuiWindowFlags_HorizontalScrollbar))
	{
		for (uint32_t i = 0; i < manip->m_ui_resource.m_texture_name.size(); i++)
		{
			char label[16] = {};
			sprintf_s(label, "texture %2d", i);

			char buf[64] = {};
			sprintf_s(buf, manip->m_ui_resource.m_texture_name[i].c_str());
			ImGui::InputText(label, buf, sizeof(buf) - 1);
			manip->m_ui_resource.m_texture_name[i] = buf;
		}

		m_request_update_texture = ImGui::Button("texture_modifi");

		ImGui::End();
	}
}

void sUIManipulater::treeWindow(std::shared_ptr<UIManipulater>& manip)
{
	ImGui::SetNextWindowSize(ImVec2(400.f, 200.f), ImGuiCond_Once);
	if (ImGui::Begin("tree", &m_is_show_tree_window))
	{
		if (m_manip)
		{
			if (ImGui::Button("addchild"))
			{
				manip->addnode(m_object_index);
			}
			treeWindow(manip, 0);
		}
		ImGui::End();
	}
}

void sUIManipulater::treeWindow(std::shared_ptr<UIManipulater>& manip, int32_t index)
{
	if (index == -1) { return; }
	ImVec4 color = manip->m_ui_resource.m_object[index].m_user_id != 0 ? ImVec4(1.f, 0.7f, 0.7f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f);
	ImGui::PushStyleColor(ImGuiCol_Text, color);

	char name[256];
	sprintf_s(name, "%s : %d", manip->m_ui_resource.m_object_tool[index].m_object_name.c_str(), index);

	bool is_open = ImGui::TreeNodeEx("objtree", m_object_index == index ? ImGuiTreeNodeFlags_Selected | ImGuiTreeNodeFlags_OpenOnArrow : 0, name);
	ImGui::PopStyleColor();
	if (ImGui::IsItemClicked(0)) {
		m_object_index = index;
	}
	if (is_open)
	{
		treeWindow(manip, manip->m_ui_resource.m_object[index].m_child_index);
		ImGui::TreePop();
	}
	treeWindow(manip, manip->m_ui_resource.m_object[index].m_sibling_index);
}

void sUIManipulater::animeWindow(std::shared_ptr<rUIAnime>& anime)
{
	// アニメーションのウインドウ
	ImGui::SetNextWindowPos(ImVec2(10.f, 220.f), ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(800.f, 400.f), ImGuiCond_Once);

	if (ImGui::Begin("anime", &m_is_show_anime_window))
	{
		if (m_anime)
		{
	// 		if (ImGui::SmallButton(m_anim_manip->m_is_playing ? "STOP" : "PLAY")) {
	// 			m_anim_manip->m_is_playing = !m_anim_manip->m_is_playing;
	// 		}
	//		ImGui::SameLine();
	// 		int fps = m_ui_anime_resource.m_info.m_target_frame;
	// 		ImGui::DragInt("Target FPS", &fps, 0.1f, 30, 300, "%3.f");
	// 		m_ui_anime_resource.m_info.m_target_frame = fps;

			int max_frame = anime->m_info.m_anime_max_frame;
			ImGui::DragInt("Anime Frame", &max_frame, 0.1f, 30, 300, "%3.f");
			anime->m_info.m_target_frame = max_frame;

			ImGui::Separator();

			ImGui::BeginChild("anime keys", ImVec2(200, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
			if (ImGui::BeginPopupContextWindow("anime key context"))
			{
				if (ImGui::Selectable("Sort"))
				{
				}
				ImGui::EndPopup();
			}

			for (int i = 0; i < anime->m_key.size(); i++)
			{
				auto& key = anime->m_key[i];
				char buf[256] = {};
				sprintf_s(buf, "target_%d", key.m_info.m_target_index);
				if (ImGui::Selectable(buf)) {
					m_anime_index = i;
				}
			}
			if (ImGui::Button("MakeData"))
			{
				UIAnimeKey new_key;
				new_key.m_info.m_type = 0;
				new_key.m_info.m_target_index = 0;
				UIAnimeKeyData new_data;
				new_data.m_frame = 0;
				new_data.m_value_i = 0;
				new_data.m_flag = UIAnimeKeyData::is_enable;
				new_key.m_data[new_key.m_info.m_type].push_back(new_data);
				anime->m_key.push_back(new_key);
			}
			ImGui::EndChild();
			ImGui::SameLine();

			ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
			animedataManip(anime);
			ImGui::EndChild();
		}
		ImGui::End();
	}
}

void sUIManipulater::animedataManip(std::shared_ptr<rUIAnime>& anime)
{
	static int current_data_type;
	const char* types[] = { "pos", "size", "color", "disable order" };
	static_assert(array_length(types) == UIAnimeKeyInfo::type_num, "");

	auto* anime_keys = m_anime_index >= 0 ? &anime->m_key[m_anime_index] : nullptr;
	if (anime_keys)
	{
		{

			ImGui::Combo("anime data type", &current_data_type, types, array_length(types));
		}
		{
			int _target = anime_keys->m_info.m_target_index;
			ImGui::DragInt("anime target index", &_target, 1.f, 0, 1024);
			anime_keys->m_info.m_target_index = _target;
		}

		auto& keys = anime_keys->m_data[current_data_type];
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
				//					m_request_update_animation = true;
			};
			ImGui::EndPopup();
		}


		ImGui::Columns(5, "animcolumns");
		ImGui::Text("Frame"); ImGui::NextColumn();
		ImGui::Text("Value"); ImGui::NextColumn();
		ImGui::Text("Enable"); ImGui::NextColumn();
		ImGui::Text("Erase"); ImGui::NextColumn();
		ImGui::Text("Interp"); ImGui::NextColumn();
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
				vec2 value_f = glm::unpackHalf2x16(key.m_value_u);
				int value[2] = { value_f[0], value_f[1] };
				ImGui::DragInt2("##value", value, 0.1f);
				key.m_value_u = glm::packHalf2x16(vec2(value[0], value[1]));
				break;
			}
			case UIAnimeKeyInfo::type_color_rgba:
			{
				vec4 value_f = glm::unpackSnorm4x8(key.m_value_u);
				int value[] = { value_f[0], value_f[1], value_f[2], value_f[3] };
				ImGui::DragInt4("##value", value, 0.1f, -127, 127);
				key.m_value_u = glm::packSnorm4x8(vec4(value[0], value[1], value[2], value[3]));
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

			sprintf_s(id, "interp_type_%d", i);
			ImGui::PushID(id);
			int interp =
				btr::isOn(key.m_flag, UIAnimeKeyData::interp_spline) ?
				1 :
				btr::isOn(key.m_flag, UIAnimeKeyData::interp_bezier) ?
				2 : 0;

			const char* interp_name[] = { "liner", "spline", "bezier" };
			ImGui::Combo("##interp", &interp, interp_name, array_length(interp_name));

			switch (interp)
			{
			default:
				btr::setOff(key.m_flag, UIAnimeKeyData::interp_spline);
				btr::setOff(key.m_flag, UIAnimeKeyData::interp_bezier);
				break;
			case 1:
				btr::setOff(key.m_flag, UIAnimeKeyData::interp_bezier);
				btr::setOn(key.m_flag, UIAnimeKeyData::interp_spline);
				break;
			case 2:
				btr::setOff(key.m_flag, UIAnimeKeyData::interp_spline);
				btr::setOn(key.m_flag, UIAnimeKeyData::interp_bezier);
				break;
			}

			ImGui::NextColumn();
			ImGui::PopID();

			ImGui::PopID();
			ImGui::Separator();
		}
		ImGui::Columns(1);
	}
	else
	{
		if (ImGui::BeginPopupContextWindow("key context"))
		{
			if (ImGui::Selectable("Make Data"))
			{
				UIAnimeKey new_key;
				new_key.m_info.m_type = current_data_type;
				new_key.m_info.m_target_index = anime->m_key[m_anime_index].m_info.m_target_index;
				UIAnimeKeyData new_data;
				new_data.m_frame = 0;
				new_data.m_value_i = 0;
				new_data.m_flag = UIAnimeKeyData::is_enable;
				new_key.m_data[new_key.m_info.m_type].push_back(new_data);
				anime->m_key.push_back(new_key);
			}
			ImGui::EndPopup();
		}

	}
}
