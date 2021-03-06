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
	glm::mat4			mTransformation;	///! 
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
	btr::BufferMemory m_vertex_buffer;
	btr::BufferMemory m_index_buffer;
	btr::BufferMemory m_indirect_buffer;
	vk::IndexType mIndexType;
	int32_t mIndirectCount; //!< メッシュの数

	std::vector<vk::VertexInputBindingDescription> m_vertex_input_binding;
	std::vector<vk::VertexInputAttributeDescription> m_vertex_input_attribute;
	vk::PipelineVertexInputStateCreateInfo m_vertex_input_info;

	void draw(vk::CommandBuffer cmd)const override;
};


class cModel
{
protected:

public:
	struct Vertex {
		enum {
			BONE_NUM = 4,
		};
		vec3		m_position;
		uint32_t	m_normal;
		uint32_t	m_texcoord0;
		u8vec4		m_bone_ID;	//!< 
		u8vec4		m_weight;
	};

	struct Material {
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
		glm::vec4 mAABB;	// xyz: center w:radius

		Mesh()
			: m_vertex_num(0)
			, m_material_index(-1)
			, m_node_index(-1)
		{}
	};


	struct Bone {
		std::string mName;
		glm::mat4 mOffset;
		int mNodeIndex;

		Bone()
			: mNodeIndex(-1)
		{}
		bool nameCompare(const std::string& name) {
			return mName.compare(name) == 0;
		}
	};
	struct ModelInfo {
		s32 mNodeNum;
		s32 mBoneNum;
		s32 mMeshNum;
		s32 m_node_depth_max;

		glm::vec4 mAabb;
		glm::mat4 mInvGlobalMatrix;
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
			vk::VertexInputBindingDescription()
			.setBinding(0)
			.setInputRate(vk::VertexInputRate::eVertex)
			.setStride(sizeof(cModel::Vertex))
		};
	}
	static std::vector<vk::VertexInputAttributeDescription> GetVertexInputAttribute()
	{
		return std::vector<vk::VertexInputAttributeDescription>
		{
			// pos
			vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(0)
			.setFormat(vk::Format::eR32G32B32Sfloat)
			.setOffset(0),
			// normal
			vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(1)
			.setFormat(vk::Format::eA2R10G10B10SnormPack32)
			.setOffset(12),
			// texcoord
			vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(2)
			.setFormat(vk::Format::eA2R10G10B10SnormPack32)
			.setOffset(16),
				// boneID
			vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(3)
			.setFormat(vk::Format::eR8G8B8A8Uint)
			.setOffset(20),
			vk::VertexInputAttributeDescription()
			.setBinding(0)
			.setLocation(4)
			.setFormat(vk::Format::eR8G8B8A8Unorm)
			.setOffset(24),
		};

	}

public:
	void load(const std::shared_ptr<btr::Context>& loader, const std::string& filename);
	void load2(const std::shared_ptr<btr::Context>& loader, const std::string& filename);

	std::string getFilename()const;
	const ResourceVertex* getMesh()const;
	std::shared_ptr<Resource> getResource()const { return m_resource; }
};

