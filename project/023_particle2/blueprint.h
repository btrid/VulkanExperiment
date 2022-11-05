#pragma once

#include <imgui-node-editor/imgui_node_editor.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <utility>

#include "utilities/builders.h"
#include "utilities/widgets.h"

namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;

using namespace ax;

using ax::Widgets::IconType;

static ed::EditorContext* m_Editor = nullptr;

enum class PinType
{
	Flow,
	Bool,
	Int,
	Float,
	String,
	Object,
	Function,
	Delegate,
};

enum class PinKind
{
	Output,
	Input
};

enum class NodeType
{
	Blueprint,
	Simple,
	Tree,
	Comment,
	Houdini
};

struct Node;

struct Pin
{
	ed::PinId   ID;
	::Node* Node;
	std::string Name;
	PinType     Type;
	PinKind     Kind;

	Pin(int id, const char* name, PinType type) :
		ID(id), Node(nullptr), Name(name), Type(type), Kind(PinKind::Input)
	{
	}
};

struct Node
{
	ed::NodeId ID;
	std::string Name;
	std::vector<Pin> Inputs;
	std::vector<Pin> Outputs;
	ImColor Color;
	NodeType Type;
	ImVec2 Size;

	std::string State;
	std::string SavedState;

	Node(int id, const char* name, ImColor color = ImColor(255, 255, 255)) :
		ID(id), Name(name), Color(color), Type(NodeType::Blueprint), Size(0, 0)
	{
	}
};

struct Link
{
	ed::LinkId ID;

	ed::PinId StartPinID;
	ed::PinId EndPinID;

	ImColor Color;

	Link(ed::LinkId id, ed::PinId startPinId, ed::PinId endPinId) :
		ID(id), StartPinID(startPinId), EndPinID(endPinId), Color(255, 255, 255)
	{
	}
};

struct NodeIdLess
{
	bool operator()(const ed::NodeId& lhs, const ed::NodeId& rhs) const
	{
		return lhs.AsPointer() < rhs.AsPointer();
	}
};

struct Blueprint
{

	int GetNextId();

	//ed::NodeId GetNextNodeId()
	//{
	//    return ed::NodeId(GetNextId());
	//}

	ed::LinkId GetNextLinkId();

	void TouchNode(ed::NodeId id);

	float GetTouchProgress(ed::NodeId id);

	void UpdateTouch();

	Node* FindNode(ed::NodeId id);

	Link* FindLink(ed::LinkId id);

	Pin* FindPin(ed::PinId id);

	bool IsPinLinked(ed::PinId id);

	bool CanCreateLink(Pin* a, Pin* b);


	void BuildNode(Node* node);

	Node* SpawnInputActionNode();

	Node* SpawnBranchNode();

	Node* SpawnDoNNode();

	Node* SpawnOutputActionNode();

	Node* SpawnPrintStringNode();

	Node* SpawnMessageNode();

	Node* SpawnSetTimerNode();

	Node* SpawnLessNode();

	Node* SpawnWeirdNode();

	Node* SpawnTraceByChannelNode();

	Node* SpawnTreeSequenceNode();

	Node* SpawnTreeTaskNode();

	Node* SpawnTreeTask2Node();

	Node* SpawnComment();

	Node* SpawnHoudiniTransformNode();

	Node* SpawnHoudiniGroupNode();

	void BuildNodes();

	Blueprint();

	~Blueprint();

	ImColor GetIconColor(PinType type);;

	void DrawPinIcon(const Pin& pin, bool connected, int alpha);;

	void ShowStyleEditor(bool* show = nullptr);

	void ShowLeftPane(float paneWidth);

	void OnFrame(float deltaTime);

	int                  m_NextId = 1;
	const int            m_PinIconSize = 24;
	std::vector<Node>    m_Nodes;
	std::vector<Link>    m_Links;
	ImTextureID          m_HeaderBackground = nullptr;
	ImTextureID          m_SaveIcon = nullptr;
	ImTextureID          m_RestoreIcon = nullptr;
	const float          m_TouchTime = 1.0f;
	std::map<ed::NodeId, float, NodeIdLess> m_NodeTouchTime;
	bool                 m_ShowOrdinals = false;
};
