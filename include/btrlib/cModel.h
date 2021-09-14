#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <future>

#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <btrlib/rTexture.h>
#include <btrlib/ResourceManager.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/cMotion.h>

class Node
{
public:
	int					mParent;
	int					m_bone_index;
	std::vector<int>	mChildren;
	std::string			mName;
	mat4			mTransformation;	///! 
	Node()
		: mParent(-1)
		, m_bone_index(-1)
	{}
};

struct RootNode
{
	std::vector<Node>	mNodeList;
	uint32_t m_depth_max;

	const Node* getNodeByName(const std::string& name)const 
	{
		for (auto& n : mNodeList) 
		{
			if (n.mName == name) 
			{
				return &n;
			}
		}
		return nullptr;
	}
	int getNodeIndexByName(const std::string& name)const 
	{
		for (unsigned i = 0; i < mNodeList.size(); i++) 
		{
			if (mNodeList[i].mName == name) 
			{
				return i;
			}
		}
		return -1;
	}
};


class cModel
{
public:
	enum {
		BONE_NUM = 4,
	};

	struct Material 
	{
		vec4		mAmbient;
		vec4		mDiffuse;
		vec4		mSpecular;
		ResourceTexture	mDiffuseTex;
		ResourceTexture	mAmbientTex;
		ResourceTexture	mNormalTex;
		ResourceTexture	mSpecularTex;
		ResourceTexture	mHeightTex;
		ResourceTexture	mReflectionTex;
		float			mShininess;
		vec4		mEmissive;
	};


	struct Mesh
	{
		vk::DrawIndexedIndirectCommand m_draw_cmd;
		s32 m_vertex_num;
		s32 m_node_index;	//!< 
		s32 m_material_index;
		vec4 mAABB;	// xyz: center w:radius

		Mesh()
			: m_vertex_num(0)
			, m_material_index(-1)
			, m_node_index(-1)
		{}
	};


	struct Bone 
	{
		std::string mName;
		mat4 mOffset;
		int mNodeIndex;

		Bone()
			: mNodeIndex(-1)
		{}
		bool nameCompare(const std::string& name) {
			return mName.compare(name) == 0;
		}
	};
	struct ModelInfo 
	{
		int32_t mNodeNum;
		int32_t mBoneNum;
		int32_t mMeshNum;
		int32_t m_node_depth_max;

		vec4 mAabb;
		mat4 mInvGlobalMatrix;
	};

	struct VertexResource
	{
		btr::BufferMemoryEx<vec3> v_position;
		btr::BufferMemoryEx<uint32_t> v_normal;
		btr::BufferMemoryEx<vec3> v_texcoord;
		btr::BufferMemoryEx<u8vec4> v_bone_id;
		btr::BufferMemoryEx<u8vec4> v_bone_weight;
		btr::BufferMemoryEx<uvec3> v_index;
		btr::BufferMemoryEx<Mesh> v_indirect;

		vk::IndexType mIndexType;
		int32_t mIndirectCount; //!< ƒƒbƒVƒ…‚Ì”
	};


	struct Resource
	{
		std::string m_filename;

		ModelInfo m_model_info;
		RootNode mNodeRoot;
		std::vector<Bone> mBone;
		std::vector<Mesh> m_mesh;
		std::vector<Material> m_material;

		VertexResource m_vertex;
		cAnimation m_animation;


		cAnimation& getAnimation() { return m_animation; }
	};
	std::shared_ptr<Resource> m_resource;

	static ResourceManager<Resource> s_manager;

public:
	void load(const std::shared_ptr<btr::Context>& loader, const std::string& filename);

	std::string getFilename()const;
	const VertexResource* getMesh()const;
	std::shared_ptr<Resource> getResource()const { return m_resource; }
};

