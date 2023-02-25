#include "blueprint.h"

static ed::NodeId contextNodeId = 0;
static ed::LinkId contextLinkId = 0;
static ed::PinId  contextPinId = 0;
static bool createNewNode = false;
static Pin* newNodeLinkPin = nullptr;
static Pin* newLinkPin = nullptr;

static float leftPaneWidth = 400.0f;
static float rightPaneWidth = 800.0f;

PinDrawInfo g_PinDrawInfo[] =
{
	{ImColor(255, 255, 255),IconType::Flow},
	{ImColor(220, 48, 48),  IconType::Circle},
	{ImColor(68, 201, 156), IconType::Circle},
	{ImColor(147, 226, 74), IconType::Circle},
	{ImColor(124, 21, 153), IconType::Circle},
	{ImColor(51, 150, 215), IconType::Circle},
	{ImColor(218, 0, 183),  IconType::Circle},
	{ImColor(255, 48, 48),  IconType::Square},
	{ImColor(147, 226, 74), IconType::Circle},
};
//static_assert(std::size(g_PinDrawInfo) == (int)PinType::Num);

ImTextureID LoadTexture(const char* path)
{
// 	int width = 0, height = 0, component = 0;
// 	if (auto data = stbi_load(path, &width, &height, &component, 4))
// 	{
// 		auto texture = CreateTexture(data, width, height);
// 		stbi_image_free(data);
// 		return texture;
// 	}
// 	else
		return nullptr;
}

ImTextureID CreateTexture(const void* data, int width, int height)
{
    return nullptr;// m_Renderer->CreateTexture(data, width, height);
}

void DestroyTexture(ImTextureID texture)
{
//	m_Renderer->DestroyTexture(texture);
}

int GetTextureWidth(ImTextureID texture)
{
    return 16;
//	return m_Renderer->GetTextureWidth(texture);
}


int GetTextureHeight(ImTextureID texture)
{
    return 16;
//	return m_Renderer->GetTextureHeight(texture);
}

static inline ImRect ImGui_GetItemRect()
{
    return ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
}

static inline ImRect ImRect_Expanded(const ImRect& rect, float x, float y)
{
    auto result = rect;
    result.Min.x -= x;
    result.Min.y -= y;
    result.Max.x += x;
    result.Max.y += y;
    return result;
}


static bool Splitter(bool split_vertically, float thickness, float* size1, float* size2, float min_size1, float min_size2, float splitter_long_axis_size = -1.0f)
{
    using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = g.CurrentWindow;
    ImGuiID id = window->GetID("##Splitter");
    ImRect bb;
    bb.Min = window->DC.CursorPos + (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
    bb.Max = bb.Min + CalcItemSize(split_vertically ? ImVec2(thickness, splitter_long_axis_size) : ImVec2(splitter_long_axis_size, thickness), 0.0f, 0.0f);
    return SplitterBehavior(bb, id, split_vertically ? ImGuiAxis_X : ImGuiAxis_Y, size1, size2, min_size1, min_size2, 0.0f);
}


int GetNextId()
{
	static int g_NextId;
	return ++g_NextId;
}

ed::LinkId GetNextLinkId()
{
	return ed::LinkId(GetNextId());
}

void Blueprint::TouchNode(ed::NodeId id)
{
	m_NodeTouchTime[id] = m_TouchTime;
}

float Blueprint::GetTouchProgress(ed::NodeId id)
{
	auto it = m_NodeTouchTime.find(id);
	if (it != m_NodeTouchTime.end() && it->second > 0.0f)
		return (m_TouchTime - it->second) / m_TouchTime;
	else
		return 0.0f;
}

void Blueprint::UpdateTouch()
{
	const auto deltaTime = ImGui::GetIO().DeltaTime;
	for (auto& entry : m_NodeTouchTime)
	{
		if (entry.second > 0.0f)
			entry.second -= deltaTime;
	}
}

Node* Blueprint::FindNode(ed::NodeId id)
{
	for (auto& node : m_Nodes)
		if (node->ID == id)
			return node.get();

	return nullptr;
}

Link* Blueprint::FindLink(ed::LinkId id)
{
	for (auto& link : m_Links)
		if (link.ID == id)
			return &link;

	return nullptr;
}

Pin* Blueprint::FindPin(ed::PinId id)
{
	if (!id)
		return nullptr;

	for (auto& node : m_Nodes)
	{
		for (auto& pin : node->Inputs)
			if (pin.ID == id)
				return &pin;

		for (auto& pin : node->Outputs)
			if (pin.ID == id)
				return &pin;
	}

	return nullptr;
}

bool Blueprint::IsPinLinked(ed::PinId id)
{
	if (!id)
		return false;

	for (auto& link : m_Links)
		if (link.StartPinID == id || link.EndPinID == id)
			return true;

	return false;
}

bool Blueprint::CanCreateLink(Pin* a, Pin* b)
{
	if (!a || !b || a == b || a->Kind == b->Kind || a->Type != b->Type || a->Node == b->Node)
		return false;

	return true;
}

void Blueprint::BuildNode(Node* node)
{
	for (auto& input : node->Inputs)
	{
		input.Node = node;
		input.Kind = PinKind::Input;
	}

	for (auto& output : node->Outputs)
	{
		output.Node = node;
		output.Kind = PinKind::Output;
	}
}

Node* Blueprint::SpawnComment()
{
	auto node = std::make_unique<Node>(GetNextId(), "Test Comment");
	node->Type = NodeType::Comment;
	node->Size = ImVec2(300, 200);

	m_Nodes.push_back(std::move(node));
	return m_Nodes.back().get();
}

Node* Blueprint::SpawnDynamicOutputNode()
{
	auto node = std::make_unique<Node>(GetNextId(), "Dynamic");
	node->Type = NodeType::DescriptorSets;
	node->Size = ImVec2(300, 200);
	node->Outputs.emplace_back(GetNextId(), "flow", PinType::Flow);
	node->Outputs.emplace_back(GetNextId(), "b", PinType::Bool);
	node->Outputs.emplace_back(GetNextId(), "f", PinType::Float);

	auto method = [&, node = node.get()](util::BlueprintNodeBuilder builder)
	{
		builder.Begin(node->ID);
		builder.Header(node->Color);
		ImGui::Spring(0);
		ImGui::TextUnformatted(node->Name.c_str());
		ImGui::Spring(1);
		ImGui::Dummy(ImVec2(0, 28));
		ImGui::Spring(0);
		builder.EndHeader();

		for (auto& input : node->Inputs)
		{
			auto alpha = ImGui::GetStyle().Alpha;
			if (newLinkPin && !CanCreateLink(newLinkPin, &input) && &input != newLinkPin)
				alpha = alpha * (48.0f / 255.0f);

			builder.Input(input.ID);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
			DrawPinIcon(input, IsPinLinked(input.ID), (int)(alpha * 255));
			ImGui::Spring(0);

			ImGui::PopStyleVar();
			builder.EndInput();
		}

		for (auto& output : node->Outputs)
		{
			auto alpha = ImGui::GetStyle().Alpha;
			if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
				alpha = alpha * (48.0f / 255.0f);

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

			builder.Output(output.ID);
//			ImGui::Spring(0);

			{
				char buf[32] = {};
				strcpy_s(buf, output.Class.c_str());
				ImGui::PushItemWidth(60.f);
				if (ImGui::InputText("##class", buf, sizeof(buf))) { output.Class = buf; }

				strcpy_s(buf, output.Name.c_str());
				ImGui::PushItemWidth(120.f);
				if (ImGui::InputText("##name", buf, sizeof(buf))) { output.Name = buf; }
				//				ImGui::Spring(0);
			}

			DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
			ImGui::PopStyleVar();
			builder.EndOutput();
		}

		builder.End();

	};
	node->methodBuild.push_back(method);

	BuildNode(node.get());
	m_Nodes.push_back(std::move(node));
	return m_Nodes.back().get();
}
Node* Blueprint::DefineDescriptorSets()
{
	auto node = std::make_unique<Node>(GetNextId(), "Description");
	node->Type = NodeType::DescriptorSets;
	node->Size = ImVec2(300, 200);
	node->Outputs.emplace_back(GetNextId(), "aaa",PinType::Bool);

	auto method = [&, node = node.get()](util::BlueprintNodeBuilder builder)
	{
		builder.Begin(node->ID);
		builder.Header(node->Color);
		ImGui::Spring(0);
		ImGui::TextUnformatted(node->Name.c_str());
		ImGui::Spring(1);
		ImGui::Dummy(ImVec2(0, 28));
			ImGui::Spring(0);
		builder.EndHeader();

		for (auto& input : node->Inputs)
		{
			auto alpha = ImGui::GetStyle().Alpha;
			if (newLinkPin && !CanCreateLink(newLinkPin, &input) && &input != newLinkPin)
				alpha = alpha * (48.0f / 255.0f);

			builder.Input(input.ID);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
			DrawPinIcon(input, IsPinLinked(input.ID), (int)(alpha * 255));
			ImGui::Spring(0);

			if (!input.Name.empty())
			{
				ImGui::TextUnformatted(input.Name.c_str());
				ImGui::Spring(0);
			}
			ImGui::PopStyleVar();
			builder.EndInput();
		}

		for (auto& output : node->Outputs)
		{
			auto alpha = ImGui::GetStyle().Alpha;
			if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
				alpha = alpha * (48.0f / 255.0f);

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
			builder.Output(output.ID);
			ImGui::Spring(0);
			DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
			ImGui::PopStyleVar();
			builder.EndOutput();
		}

		builder.End();

	};
	node->methodBuild.push_back(method);

	BuildNode(node.get());

	m_Nodes.push_back(std::move(node));
	return m_Nodes.back().get();

}

void Blueprint::BuildNodes()
{
	for (auto& node : m_Nodes)
		BuildNode(node.get());
}

Blueprint::Blueprint()
{
	ed::Config config;

	config.SettingsFile = "Blueprints.json";

	config.UserPointer = this;

	config.LoadNodeSettings = [](ed::NodeId nodeId, char* data, void* userPointer) -> size_t
	{
		auto self = static_cast<Blueprint*>(userPointer);

		auto node = self->FindNode(nodeId);
		if (!node)
			return 0;

		if (data != nullptr)
			memcpy(data, node->State.data(), node->State.size());
		return node->State.size();
	};

	config.SaveNodeSettings = [](ed::NodeId nodeId, const char* data, size_t size, ed::SaveReasonFlags reason, void* userPointer) -> bool
	{
		auto self = static_cast<Blueprint*>(userPointer);

		auto node = self->FindNode(nodeId);
		if (!node)
			return false;

		node->State.assign(data, size);

		self->TouchNode(nodeId);

		return true;
	};

	m_Editor = ed::CreateEditor(&config);
	ed::SetCurrentEditor(m_Editor);

	Node* node;
	node = SpawnDynamicOutputNode();			ed::SetNodePosition(node->ID, ImVec2(55, 152));
	node = DefineDescriptorSets();      ed::SetNodePosition(node->ID, ImVec2(55, 332));

	ed::NavigateToContent();

	BuildNodes();



	//         m_HeaderBackground = LoadTexture("blueprint/BlueprintBackground.png");
	//         m_SaveIcon         = LoadTexture("blueprint/ic_save_white_24dp.png");
	//         m_RestoreIcon      = LoadTexture("blueprint/ic_restore_white_24dp.png");
}

Blueprint::~Blueprint()
{
	auto releaseTexture = [this](ImTextureID& id)
	{
		if (id)
		{
			DestroyTexture(id);
			id = nullptr;
		}
	};

	releaseTexture(m_RestoreIcon);
	releaseTexture(m_SaveIcon);
	releaseTexture(m_HeaderBackground);

	if (m_Editor)
	{
		ed::DestroyEditor(m_Editor);
		m_Editor = nullptr;
	}
}

ImColor Blueprint::GetIconColor(PinType type)
{
	return g_PinDrawInfo[(int)type].color;
}

void Blueprint::DrawPinIcon(const Pin& pin, bool connected, int alpha)
{
	IconType iconType = g_PinDrawInfo[(int)pin.Type].type;
	ImColor  color = GetIconColor(pin.Type);
	color.Value.w = alpha / 255.0f;

	ax::Widgets::Icon(ImVec2(static_cast<float>(m_PinIconSize), static_cast<float>(m_PinIconSize)), iconType, connected, color, ImColor(32, 32, 32, alpha));
}

void Blueprint::ShowStyleEditor(bool* show /*= nullptr*/)
{
	if (!ImGui::Begin("Style", show))
	{
		ImGui::End();
		return;
	}

	auto paneWidth = ImGui::GetContentRegionAvail().x;

	auto& editorStyle = ed::GetStyle();
	ImGui::BeginHorizontal("Style buttons", ImVec2(paneWidth, 0), 1.0f);
	ImGui::TextUnformatted("Values");
	ImGui::Spring();
	if (ImGui::Button("Reset to defaults"))
		editorStyle = ed::Style();
	ImGui::EndHorizontal();
	ImGui::Spacing();
	ImGui::DragFloat4("Node Padding", &editorStyle.NodePadding.x, 0.1f, 0.0f, 40.0f);
	ImGui::DragFloat("Node Rounding", &editorStyle.NodeRounding, 0.1f, 0.0f, 40.0f);
	ImGui::DragFloat("Node Border Width", &editorStyle.NodeBorderWidth, 0.1f, 0.0f, 15.0f);
	ImGui::DragFloat("Hovered Node Border Width", &editorStyle.HoveredNodeBorderWidth, 0.1f, 0.0f, 15.0f);
	ImGui::DragFloat("Selected Node Border Width", &editorStyle.SelectedNodeBorderWidth, 0.1f, 0.0f, 15.0f);
	ImGui::DragFloat("Pin Rounding", &editorStyle.PinRounding, 0.1f, 0.0f, 40.0f);
	ImGui::DragFloat("Pin Border Width", &editorStyle.PinBorderWidth, 0.1f, 0.0f, 15.0f);
	ImGui::DragFloat("Link Strength", &editorStyle.LinkStrength, 1.0f, 0.0f, 500.0f);
	//ImVec2  SourceDirection;
	//ImVec2  TargetDirection;
	ImGui::DragFloat("Scroll Duration", &editorStyle.ScrollDuration, 0.001f, 0.0f, 2.0f);
	ImGui::DragFloat("Flow Marker Distance", &editorStyle.FlowMarkerDistance, 1.0f, 1.0f, 200.0f);
	ImGui::DragFloat("Flow Speed", &editorStyle.FlowSpeed, 1.0f, 1.0f, 2000.0f);
	ImGui::DragFloat("Flow Duration", &editorStyle.FlowDuration, 0.001f, 0.0f, 5.0f);
	//ImVec2  PivotAlignment;
	//ImVec2  PivotSize;
	//ImVec2  PivotScale;
	//float   PinCorners;
	//float   PinRadius;
	//float   PinArrowSize;
	//float   PinArrowWidth;
	ImGui::DragFloat("Group Rounding", &editorStyle.GroupRounding, 0.1f, 0.0f, 40.0f);
	ImGui::DragFloat("Group Border Width", &editorStyle.GroupBorderWidth, 0.1f, 0.0f, 15.0f);

	ImGui::Separator();

	static ImGuiColorEditFlags edit_mode = ImGuiColorEditFlags_DisplayRGB;
	ImGui::BeginHorizontal("Color Mode", ImVec2(paneWidth, 0), 1.0f);
	ImGui::TextUnformatted("Filter Colors");
	ImGui::Spring();
	ImGui::RadioButton("RGB", &edit_mode, ImGuiColorEditFlags_DisplayRGB);
	ImGui::Spring(0);
	ImGui::RadioButton("HSV", &edit_mode, ImGuiColorEditFlags_DisplayHSV);
	ImGui::Spring(0);
	ImGui::RadioButton("HEX", &edit_mode, ImGuiColorEditFlags_DisplayHex);
	ImGui::EndHorizontal();

	static ImGuiTextFilter filter;
	filter.Draw("", paneWidth);

	ImGui::Spacing();

	ImGui::PushItemWidth(-160);
	for (int i = 0; i < ed::StyleColor_Count; ++i)
	{
		auto name = ed::GetStyleColorName((ed::StyleColor)i);
		if (!filter.PassFilter(name))
			continue;

		ImGui::ColorEdit4(name, &editorStyle.Colors[i].x, edit_mode);
	}
	ImGui::PopItemWidth();

	ImGui::End();
}

void Blueprint::ShowLeftPane(float paneWidth)
{
	auto& io = ImGui::GetIO();

	ImGui::BeginChild("Selection", ImVec2(paneWidth, 0));

	paneWidth = ImGui::GetContentRegionAvail().x;

	static bool showStyleEditor = false;
	ImGui::BeginHorizontal("Style Editor", ImVec2(paneWidth, 0));
	ImGui::Spring(0.0f, 0.0f);
	if (ImGui::Button("Zoom to Content"))
		ed::NavigateToContent();
	ImGui::Spring(0.0f);
	if (ImGui::Button("Show Flow"))
	{
		for (auto& link : m_Links)
			ed::Flow(link.ID);
	}
	ImGui::Spring();
	if (ImGui::Button("Edit Style"))
		showStyleEditor = true;
	ImGui::EndHorizontal();
	ImGui::Checkbox("Show Ordinals", &m_ShowOrdinals);

	if (showStyleEditor)
		ShowStyleEditor(&showStyleEditor);

	std::vector<ed::NodeId> selectedNodes;
	std::vector<ed::LinkId> selectedLinks;
	selectedNodes.resize(ed::GetSelectedObjectCount());
	selectedLinks.resize(ed::GetSelectedObjectCount());

	int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
	int linkCount = ed::GetSelectedLinks(selectedLinks.data(), static_cast<int>(selectedLinks.size()));

	selectedNodes.resize(nodeCount);
	selectedLinks.resize(linkCount);

	int saveIconWidth = GetTextureWidth(m_SaveIcon);
	int saveIconHeight = GetTextureWidth(m_SaveIcon);
	int restoreIconWidth = GetTextureWidth(m_RestoreIcon);
	int restoreIconHeight = GetTextureWidth(m_RestoreIcon);

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImGui::GetCursorScreenPos(),
		ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
		ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
	ImGui::Spacing(); ImGui::SameLine();
	ImGui::TextUnformatted("Nodes");
	ImGui::Indent();
	for (auto& node : m_Nodes)
	{
		ImGui::PushID(node->ID.AsPointer());
		auto start = ImGui::GetCursorScreenPos();

		if (const auto progress = GetTouchProgress(node->ID))
		{
			ImGui::GetWindowDrawList()->AddLine(
				start + ImVec2(-8, 0),
				start + ImVec2(-8, ImGui::GetTextLineHeight()),
				IM_COL32(255, 0, 0, 255 - (int)(255 * progress)), 4.0f);
		}

		bool isSelected = std::find(selectedNodes.begin(), selectedNodes.end(), node->ID) != selectedNodes.end();
		if (ImGui::Selectable((node->Name + "##" + std::to_string(reinterpret_cast<uintptr_t>(node->ID.AsPointer()))).c_str(), &isSelected))
		{
			if (io.KeyCtrl)
			{
				if (isSelected)
					ed::SelectNode(node->ID, true);
				else
					ed::DeselectNode(node->ID);
			}
			else
				ed::SelectNode(node->ID, false);

			ed::NavigateToSelection();
		}
		if (ImGui::IsItemHovered() && !node->State.empty())
			ImGui::SetTooltip("State: %s", node->State.c_str());

		auto id = std::string("(") + std::to_string(reinterpret_cast<uintptr_t>(node->ID.AsPointer())) + ")";
		auto textSize = ImGui::CalcTextSize(id.c_str(), nullptr);
		auto iconPanelPos = start + ImVec2(
			paneWidth - ImGui::GetStyle().FramePadding.x - ImGui::GetStyle().IndentSpacing - saveIconWidth - restoreIconWidth - ImGui::GetStyle().ItemInnerSpacing.x * 1,
			(ImGui::GetTextLineHeight() - saveIconHeight) / 2);
		ImGui::GetWindowDrawList()->AddText(
			ImVec2(iconPanelPos.x - textSize.x - ImGui::GetStyle().ItemInnerSpacing.x, start.y),
			IM_COL32(255, 255, 255, 255), id.c_str(), nullptr);

		auto drawList = ImGui::GetWindowDrawList();
		ImGui::SetCursorScreenPos(iconPanelPos);
		ImGui::SetItemAllowOverlap();
		if (node->SavedState.empty())
		{
			if (ImGui::InvisibleButton("save", ImVec2((float)saveIconWidth, (float)saveIconHeight)))
				node->SavedState = node->State;

			if (ImGui::IsItemActive())
				drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 96));
			else if (ImGui::IsItemHovered())
				drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
			else
				drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 160));
		}
		else
		{
			ImGui::Dummy(ImVec2((float)saveIconWidth, (float)saveIconHeight));
			drawList->AddImage(m_SaveIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 32));
		}

		ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::SetItemAllowOverlap();
		if (!node->SavedState.empty())
		{
			if (ImGui::InvisibleButton("restore", ImVec2((float)restoreIconWidth, (float)restoreIconHeight)))
			{
				node->State = node->SavedState;
				ed::RestoreNodeState(node->ID);
				node->SavedState.clear();
			}

			if (ImGui::IsItemActive())
				drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 96));
			else if (ImGui::IsItemHovered())
				drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
			else
				drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 160));
		}
		else
		{
			ImGui::Dummy(ImVec2((float)restoreIconWidth, (float)restoreIconHeight));
			drawList->AddImage(m_RestoreIcon, ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 32));
		}

		ImGui::SameLine(0, 0);
		ImGui::SetItemAllowOverlap();
		ImGui::Dummy(ImVec2(0, (float)restoreIconHeight));

		ImGui::PopID();
	}
	ImGui::Unindent();

	static int changeCount = 0;

	ImGui::GetWindowDrawList()->AddRectFilled(
		ImGui::GetCursorScreenPos(),
		ImGui::GetCursorScreenPos() + ImVec2(paneWidth, ImGui::GetTextLineHeight()),
		ImColor(ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]), ImGui::GetTextLineHeight() * 0.25f);
	ImGui::Spacing(); ImGui::SameLine();
	ImGui::TextUnformatted("Selection");

	ImGui::BeginHorizontal("Selection Stats", ImVec2(paneWidth, 0));
	ImGui::Text("Changed %d time%s", changeCount, changeCount > 1 ? "s" : "");
	ImGui::Spring();
	if (ImGui::Button("Deselect All"))
		ed::ClearSelection();
	ImGui::EndHorizontal();
	ImGui::Indent();
	for (int i = 0; i < nodeCount; ++i) ImGui::Text("Node (%p)", selectedNodes[i].AsPointer());
	for (int i = 0; i < linkCount; ++i) ImGui::Text("Link (%p)", selectedLinks[i].AsPointer());
	ImGui::Unindent();

	if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Z)))
		for (auto& link : m_Links)
			ed::Flow(link.ID);

	if (ed::HasSelectionChanged())
		++changeCount;

	ImGui::EndChild();
}

void Blueprint::OnFrame(float deltaTime)
{
	auto& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(io.DisplaySize);
	const auto windowBorderSize = ImGui::GetStyle().WindowBorderSize;
	const auto windowRounding = ImGui::GetStyle().WindowRounding;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin("Content", nullptr /*,GetWindowFlags()*/);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, windowBorderSize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, windowRounding);

	UpdateTouch();

	ed::SetCurrentEditor(m_Editor);

	Splitter(true, 4.0f, &leftPaneWidth, &rightPaneWidth, 50.0f, 50.0f);

	ShowLeftPane(leftPaneWidth - 4.0f);

	ImGui::SameLine(0.0f, 12.0f);

	ed::Begin("Node editor");
	{
		auto cursorTopLeft = ImGui::GetCursorScreenPos();

		util::BlueprintNodeBuilder builder;//(/*m_HeaderBackground, GetTextureWidth(m_HeaderBackground), GetTextureHeight(m_HeaderBackground)*/);

		for (auto& node : m_Nodes)
		{
			if (node->Type != NodeType::Blueprint)
				continue;

			bool hasOutputDelegates = false;
			for (auto& output : node->Outputs)
				if (output.Type == PinType::Delegate)
					hasOutputDelegates = true;

			builder.Begin(node->ID);
			builder.Header(node->Color);
			ImGui::Spring(0);
			ImGui::TextUnformatted(node->Name.c_str());
			ImGui::Spring(1);
			ImGui::Dummy(ImVec2(0, 28));
			ImGui::Spring(0);
			builder.EndHeader();

			for (auto& input : node->Inputs)
			{
				auto alpha = ImGui::GetStyle().Alpha;
				if (newLinkPin && !CanCreateLink(newLinkPin, &input) && &input != newLinkPin)
					alpha = alpha * (48.0f / 255.0f);

				builder.Input(input.ID);
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
				DrawPinIcon(input, IsPinLinked(input.ID), (int)(alpha * 255));
				ImGui::Spring(0);
				if (!input.Name.empty())
				{
					ImGui::TextUnformatted(input.Name.c_str());
					ImGui::Spring(0);
				}
				if (input.Type == PinType::Bool)
				{
					ImGui::Button("Hello");
					ImGui::Spring(0);
				}
				ImGui::PopStyleVar();
				builder.EndInput();
			}

			for (auto& output : node->Outputs)
			{
				if (output.Type == PinType::Delegate)
					continue;

				auto alpha = ImGui::GetStyle().Alpha;
				if (newLinkPin && !CanCreateLink(newLinkPin, &output) && &output != newLinkPin)
					alpha = alpha * (48.0f / 255.0f);

				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
				builder.Output(output.ID);
				if (output.Type == PinType::String)
				{
					static char buffer[128] = "Edit Me\nMultiline!";
					static bool wasActive = false;

					ImGui::PushItemWidth(100.0f);
					ImGui::InputText("##edit", buffer, 127);
					ImGui::PopItemWidth();
					if (ImGui::IsItemActive() && !wasActive)
					{
						ed::EnableShortcuts(false);
						wasActive = true;
					}
					else if (!ImGui::IsItemActive() && wasActive)
					{
						ed::EnableShortcuts(true);
						wasActive = false;
					}
					ImGui::Spring(0);
				}
				if (!output.Name.empty())
				{
					ImGui::Spring(0);
					ImGui::TextUnformatted(output.Name.c_str());
				}
				ImGui::Spring(0);
				DrawPinIcon(output, IsPinLinked(output.ID), (int)(alpha * 255));
				ImGui::PopStyleVar();
				builder.EndOutput();
			}

			builder.End();
		}

		for (auto& node : m_Nodes)
		{
			if (node->Type != NodeType::Comment)
				continue;

			const float commentAlpha = 0.75f;

			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha);
			ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(255, 255, 255, 64));
			ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(255, 255, 255, 64));
			ed::BeginNode(node->ID);
			ImGui::PushID(node->ID.AsPointer());
			ImGui::BeginVertical("content");
			ImGui::BeginHorizontal("horizontal");
			ImGui::Spring(1);
			ImGui::TextUnformatted(node->Name.c_str());
			ImGui::Spring(1);
			ImGui::EndHorizontal();
			ed::Group(node->Size);
			ImGui::EndVertical();
			ImGui::PopID();
			ed::EndNode();
			ed::PopStyleColor(2);
			ImGui::PopStyleVar();

			if (ed::BeginGroupHint(node->ID))
			{
				//auto alpha   = static_cast<int>(commentAlpha * ImGui::GetStyle().Alpha * 255);
				auto bgAlpha = static_cast<int>(ImGui::GetStyle().Alpha * 255);

				//ImGui::PushStyleVar(ImGuiStyleVar_Alpha, commentAlpha * ImGui::GetStyle().Alpha);

				auto min = ed::GetGroupMin();
				//auto max = ed::GetGroupMax();

				ImGui::SetCursorScreenPos(min - ImVec2(-8, ImGui::GetTextLineHeightWithSpacing() + 4));
				ImGui::BeginGroup();
				ImGui::TextUnformatted(node->Name.c_str());
				ImGui::EndGroup();

				auto drawList = ed::GetHintBackgroundDrawList();

				auto hintBounds = ImGui_GetItemRect();
				auto hintFrameBounds = ImRect_Expanded(hintBounds, 8, 4);

				drawList->AddRectFilled(
					hintFrameBounds.GetTL(),
					hintFrameBounds.GetBR(),
					IM_COL32(255, 255, 255, 64 * bgAlpha / 255), 4.0f);

				drawList->AddRect(
					hintFrameBounds.GetTL(),
					hintFrameBounds.GetBR(),
					IM_COL32(255, 255, 255, 128 * bgAlpha / 255), 4.0f);

				//ImGui::PopStyleVar();
			}
			ed::EndGroupHint();
		}

		for (auto& node : m_Nodes)
		{
			for (auto f : node->methodBuild)
			{
				f(builder);
			}
		}

		for (auto& link : m_Links)
			ed::Link(link.ID, link.StartPinID, link.EndPinID, link.Color, 2.0f);

		if (!createNewNode)
		{
			if (ed::BeginCreate(ImColor(255, 255, 255), 2.0f))
			{
				auto showLabel = [](const char* label, ImColor color)
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
					auto size = ImGui::CalcTextSize(label);

					auto padding = ImGui::GetStyle().FramePadding;
					auto spacing = ImGui::GetStyle().ItemSpacing;

					ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

					auto rectMin = ImGui::GetCursorScreenPos() - padding;
					auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

					auto drawList = ImGui::GetWindowDrawList();
					drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
					ImGui::TextUnformatted(label);
				};

				ed::PinId startPinId = 0, endPinId = 0;
				if (ed::QueryNewLink(&startPinId, &endPinId))
				{
					auto startPin = FindPin(startPinId);
					auto endPin = FindPin(endPinId);

					newLinkPin = startPin ? startPin : endPin;

					if (startPin->Kind == PinKind::Input)
					{
						std::swap(startPin, endPin);
						std::swap(startPinId, endPinId);
					}

					if (startPin && endPin)
					{
						if (endPin == startPin)
						{
							ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
						}
						else if (endPin->Kind == startPin->Kind)
						{
							showLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
							ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
						}
						//else if (endPin->Node == startPin->Node)
						//{
						//    showLabel("x Cannot connect to self", ImColor(45, 32, 32, 180));
						//    ed::RejectNewItem(ImColor(255, 0, 0), 1.0f);
						//}
						else if (endPin->Type != startPin->Type)
						{
							showLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
							ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
						}
						else
						{
							showLabel("+ Create Link", ImColor(32, 45, 32, 180));
							if (ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f))
							{
								m_Links.emplace_back(Link(GetNextId(), startPinId, endPinId));
								m_Links.back().Color = GetIconColor(startPin->Type);
							}
						}
					}
				}

				ed::PinId pinId = 0;
				if (ed::QueryNewNode(&pinId))
				{
					newLinkPin = FindPin(pinId);
					if (newLinkPin)
						showLabel("+ Create Node", ImColor(32, 45, 32, 180));

					if (ed::AcceptNewItem())
					{
						createNewNode = true;
						newNodeLinkPin = FindPin(pinId);
						newLinkPin = nullptr;
						ed::Suspend();
						ImGui::OpenPopup("Create New Node");
						ed::Resume();
					}
				}
			}
			else
				newLinkPin = nullptr;

			ed::EndCreate();

			if (ed::BeginDelete())
			{
				ed::LinkId linkId = 0;
				while (ed::QueryDeletedLink(&linkId))
				{
					if (ed::AcceptDeletedItem())
					{
						auto id = std::find_if(m_Links.begin(), m_Links.end(), [linkId](auto& link) { return link.ID == linkId; });
						if (id != m_Links.end())
							m_Links.erase(id);
					}
				}

				ed::NodeId nodeId = 0;
				while (ed::QueryDeletedNode(&nodeId))
				{
					if (ed::AcceptDeletedItem())
					{
						auto id = std::find_if(m_Nodes.begin(), m_Nodes.end(), [nodeId](auto& node) { return node->ID == nodeId; });
						if (id != m_Nodes.end())
							m_Nodes.erase(id);
					}
				}
			}
			ed::EndDelete();
		}

		ImGui::SetCursorScreenPos(cursorTopLeft);
	}

# if 1
	auto openPopupPosition = ImGui::GetMousePos();
	ed::Suspend();
	if (ed::ShowNodeContextMenu(&contextNodeId))
		ImGui::OpenPopup("Node Context Menu");
	else if (ed::ShowPinContextMenu(&contextPinId))
		ImGui::OpenPopup("Pin Context Menu");
	else if (ed::ShowLinkContextMenu(&contextLinkId))
		ImGui::OpenPopup("Link Context Menu");
	else if (ed::ShowBackgroundContextMenu())
	{
		ImGui::OpenPopup("Create New Node");
		newNodeLinkPin = nullptr;
	}
	ed::Resume();

	ed::Suspend();
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	if (ImGui::BeginPopup("Node Context Menu"))
	{
		auto node = FindNode(contextNodeId);

		ImGui::TextUnformatted("Node Context Menu");
		ImGui::Separator();
		if (node)
		{
			ImGui::Text("ID: %p", node->ID.AsPointer());
			ImGui::Text("Type: %s", node->Type == NodeType::Blueprint ? "Blueprint" : "Comment");
			ImGui::Text("Inputs: %d", (int)node->Inputs.size());
			ImGui::Text("Outputs: %d", (int)node->Outputs.size());
		}
		else
			ImGui::Text("Unknown node: %p", contextNodeId.AsPointer());
		ImGui::Separator();
		if (ImGui::MenuItem("Delete"))
			ed::DeleteNode(contextNodeId);
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Pin Context Menu"))
	{
		auto pin = FindPin(contextPinId);

		ImGui::TextUnformatted("Pin Context Menu");
		ImGui::Separator();
		if (pin)
		{
			ImGui::Text("ID: %p", pin->ID.AsPointer());
			if (pin->Node)
				ImGui::Text("Node: %p", pin->Node->ID.AsPointer());
			else
				ImGui::Text("Node: %s", "<none>");
		}
		else
			ImGui::Text("Unknown pin: %p", contextPinId.AsPointer());

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Link Context Menu"))
	{
		auto link = FindLink(contextLinkId);

		ImGui::TextUnformatted("Link Context Menu");
		ImGui::Separator();
		if (link)
		{
			ImGui::Text("ID: %p", link->ID.AsPointer());
			ImGui::Text("From: %p", link->StartPinID.AsPointer());
			ImGui::Text("To: %p", link->EndPinID.AsPointer());
		}
		else
			ImGui::Text("Unknown link: %p", contextLinkId.AsPointer());
		ImGui::Separator();
		if (ImGui::MenuItem("Delete"))
			ed::DeleteLink(contextLinkId);
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("Create New Node"))
	{
		auto newNodePostion = openPopupPosition;
		//ImGui::SetCursorScreenPos(ImGui::GetMousePosOnOpeningCurrentPopup());

		//auto drawList = ImGui::GetWindowDrawList();
		//drawList->AddCircleFilled(ImGui::GetMousePosOnOpeningCurrentPopup(), 10.0f, 0xFFFF00FF);

		Node* node = nullptr;
		ImGui::Separator();
		if (ImGui::MenuItem("Dynamic"))
			node = SpawnDynamicOutputNode();
		if (ImGui::MenuItem("Description"))
			node = DefineDescriptorSets();

		if (node)
		{
			BuildNodes();

			createNewNode = false;

			ed::SetNodePosition(node->ID, newNodePostion);

			if (auto startPin = newNodeLinkPin)
			{
				auto& pins = startPin->Kind == PinKind::Input ? node->Outputs : node->Inputs;

				for (auto& pin : pins)
				{
					if (CanCreateLink(startPin, &pin))
					{
						auto endPin = &pin;
						if (startPin->Kind == PinKind::Input)
							std::swap(startPin, endPin);

						m_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
						m_Links.back().Color = GetIconColor(startPin->Type);

						break;
					}
				}
			}
		}

		ImGui::EndPopup();
	}
	else
		createNewNode = false;
	ImGui::PopStyleVar();
	ed::Resume();
# endif

	ed::End();

	auto editorMin = ImGui::GetItemRectMin();
	auto editorMax = ImGui::GetItemRectMax();

	if (m_ShowOrdinals)
	{
		int nodeCount = ed::GetNodeCount();
		std::vector<ed::NodeId> orderedNodeIds;
		orderedNodeIds.resize(static_cast<size_t>(nodeCount));
		ed::GetOrderedNodeIds(orderedNodeIds.data(), nodeCount);


		auto drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(editorMin, editorMax);

		int ordinal = 0;
		for (auto& nodeId : orderedNodeIds)
		{
			auto p0 = ed::GetNodePosition(nodeId);
			auto p1 = p0 + ed::GetNodeSize(nodeId);
			p0 = ed::CanvasToScreen(p0);
			p1 = ed::CanvasToScreen(p1);


			ImGuiTextBuffer builder;
			builder.appendf("#%d", ordinal++);

			auto textSize = ImGui::CalcTextSize(builder.c_str());
			auto padding = ImVec2(2.0f, 2.0f);
			auto widgetSize = textSize + padding * 2;

			auto widgetPosition = ImVec2(p1.x, p0.y) + ImVec2(0.0f, -widgetSize.y);

			drawList->AddRectFilled(widgetPosition, widgetPosition + widgetSize, IM_COL32(100, 80, 80, 190), 3.0f, ImDrawFlags_RoundCornersAll);
			drawList->AddRect(widgetPosition, widgetPosition + widgetSize, IM_COL32(200, 160, 160, 190), 3.0f, ImDrawFlags_RoundCornersAll);
			drawList->AddText(widgetPosition + padding, IM_COL32(255, 255, 255, 255), builder.c_str());
		}

		drawList->PopClipRect();
	}


	//ImGui::ShowTestWindow();
	//ImGui::ShowMetricsWindow();

	ImGui::PopStyleVar(2);
	ImGui::End();
	ImGui::PopStyleVar(2);


}
