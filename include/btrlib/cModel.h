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

public:
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


struct ResourceVertex : public Drawable
{
	btr::BufferMemoryEx<vec3> v_position;
	btr::BufferMemoryEx<uint32_t> v_normal;
	btr::BufferMemoryEx<vec3> v_texcoord;
	btr::BufferMemoryEx<u8vec4> v_bone_id;
	btr::BufferMemoryEx<u8vec4> v_bone_weight;
	btr::BufferMemoryEx<uvec3> v_index;
	btr::BufferMemory v_indirect;
	vk::IndexType mIndexType;
	int32_t mIndirectCount; //!< ƒƒbƒVƒ…‚Ì”

	void draw(vk::CommandBuffer cmd)const override;
};


class cModel
{
protected:

public:
	enum {
		BONE_NUM = 4,
	};

	struct Material 
	{
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		ResourceTexture	mDiffuseTex;
		ResourceTexture	mAmbientTex;
		ResourceTexture	mNormalTex;
		ResourceTexture	mSpecularTex;
		ResourceTexture	mHeightTex;
		ResourceTexture	mReflectionTex;
		float			mShininess;
		glm::vec4		mEmissive;
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

public:
	struct Resource
	{
		friend cModel;
	public:
		std::string m_filename;

		ModelInfo m_model_info;
		RootNode mNodeRoot;
		std::vector<Bone> mBone;
		std::vector<Mesh> m_mesh;
		std::vector<Material> m_material;

		ResourceVertex m_mesh_resource;
		cAnimation m_animation;


		cAnimation& getAnimation() { return m_animation; }
	};
	std::shared_ptr<Resource> m_resource;

	static ResourceManager<Resource> s_manager;
	static std::vector<vk::VertexInputBindingDescription> GetVertexInputBinding()
	{
		return std::vector<vk::VertexInputBindingDescription>
		{
			vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(vec3)),
			vk::VertexInputBindingDescription().setBinding(1).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(uint32_t)),
			vk::VertexInputBindingDescription().setBinding(2).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(vec3)),
			vk::VertexInputBindingDescription().setBinding(3).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(u8vec4)),
			vk::VertexInputBindingDescription().setBinding(4).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(u8vec4)),
		};
	}
	static std::vector<vk::VertexInputAttributeDescription> GetVertexInputAttribute()
	{
		return std::vector<vk::VertexInputAttributeDescription>
		{
			// pos
			vk::VertexInputAttributeDescription().setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
			// normal
			vk::VertexInputAttributeDescription().setBinding(1).setLocation(1).setFormat(vk::Format::eA2R10G10B10SnormPack32).setOffset(0),
			// texcoord
			vk::VertexInputAttributeDescription().setBinding(2).setLocation(2).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
			// boneID
			vk::VertexInputAttributeDescription().setBinding(3).setLocation(3).setFormat(vk::Format::eR8G8B8A8Uint).setOffset(0),
			vk::VertexInputAttributeDescription().setBinding(4).setLocation(4).setFormat(vk::Format::eR8G8B8A8Unorm).setOffset(0),
		};

	}

public:
	void load(const std::shared_ptr<btr::Context>& loader, const std::string& filename);

	std::string getFilename()const;
	const ResourceVertex* getMesh()const;
	std::shared_ptr<Resource> getResource()const { return m_resource; }
};

