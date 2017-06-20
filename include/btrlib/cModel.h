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
#include <btrlib/BufferMemory.h>

struct ModelLoader
{
	cDevice m_device;
	btr::BufferMemory m_vertex_memory;
	btr::BufferMemory m_storage_memory;
	btr::BufferMemory m_storage_uniform_memory;
	btr::BufferMemory m_staging_memory;

	vk::CommandBuffer m_cmd;
};

struct cMeshResource 
{
	btr::AllocatedMemory m_vertex_buffer_ex;
	btr::AllocatedMemory m_index_buffer_ex;
	btr::AllocatedMemory m_indirect_buffer_ex;
	vk::IndexType mIndexType;
	int32_t mIndirectCount;

};

class Node
{
public:
	int					mParent;
	std::vector<int>	mChildren;
	std::string			mName;
	glm::mat4			mTransformation;	///! 
	Node()
		: mParent(-1)
	{}
};

class RootNode
{

public:
	std::vector<Node>	mNodeList;

	const Node* getRootNode()const { return mNodeList.empty() ? nullptr : &mNodeList[0]; }
	Node* getNodeByIndex(int index) { return mNodeList.empty() ? nullptr : &mNodeList[index]; }
	const Node* getNodeByIndex(int index)const { return mNodeList.empty() ? nullptr : &mNodeList[index]; }
	const Node* getNodeByName(const std::string& name)const {
		for (auto& n : mNodeList) {
			if (n.mName == name) {
				return &n;
			}
		}
		return nullptr;
	}
	int getNodeIndexByName(const std::string& name)const {
		for (unsigned i = 0; i < mNodeList.size(); i++) {
			if (mNodeList[i].mName == name) {
				return i;
			}
		}
		return -1;
	}

};


struct Motion
{
	struct VecKey
	{
		float m_time;
		glm::vec3 m_data;
	};
	struct QuatKey
	{
		float m_time;
		glm::quat m_data;
	};
	struct NodeMotion
	{
		uint32_t m_node_index;
		std::string m_nodename;
		std::vector<VecKey> m_translate;
		std::vector<VecKey> m_scale;
		std::vector<QuatKey> m_rotate;
	};
	std::string m_name;
	float m_duration;
	float m_ticks_per_second;
	std::vector<NodeMotion> m_data;
};
struct cAnimation
{
	std::vector<Motion> m_motion;
};

struct ResourceTexture
{
	struct Resource 
	{
		std::string m_filename;
		cDevice m_device;
		vk::Image m_image;
		vk::ImageView m_image_view;
		vk::DeviceMemory m_memory;

		vk::Sampler m_sampler;

		~Resource()
		{
			if (m_image)
			{
// 				std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
// 				deleter->device = m_device.getHandle();
// 				deleter->image = { m_image };
// 				deleter->sampler = { m_sampler };
// 				deleter->memory = { m_memory };
// 				deleter->fence_shared = { m_fence_shared };
// 				sGlobal::Order().destroyResource(std::move(deleter));

				m_device->destroyImageView(m_image_view);
			}
		}
	};

	std::shared_ptr<Resource> m_private;

	void load(ModelLoader* loader, cThreadPool& thread_pool, const std::string& filename);
	vk::ImageView getImageView()const { return m_private ? m_private->m_image_view : vk::ImageView(); }
	vk::Sampler getSampler()const { return m_private ? m_private->m_sampler : vk::Sampler(); }

	bool isReady()const 
	{
//		return m_private ? m_private->m_device->getFenceStatus(*m_private->m_fence_shared) == vk::Result::eSuccess : true;
		return true;
	}

	static ResourceManager<Resource> s_manager;

};

class cModel
{
protected:

public:
	struct Vertex {
		enum {
			BONE_NUM = 4,
		};
		glm::vec3	m_position;
		glm::vec3	m_normal;
		s8x4		m_texcoord0;
		glm::u8vec4	m_bone_ID;	//!< 
		u8x4		m_weight;
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
		cMeshResource m_mesh_resource;
		std::vector<Mesh> m_mesh;

		RootNode mNodeRoot;
		std::vector<Bone> mBone;
		std::vector<Material>	m_material;

// 		btr::AllocatedMemory m_material_gpu;
// 		btr::AllocatedMemory m_node_info_gpu;
// 		btr::AllocatedMemory m_bone_info_gpu;
// 
// 		btr::UpdateBufferEx m_world_staging_buffer;
// 		btr::UpdateBufferEx m_bone_staging_buffer;
// 		btr::AllocatedMemory m_model_info_staging_buffer;

		cAnimation m_animation;

		ModelInfo m_model_info;

		cAnimation& getAnimation() { return m_animation; }

		Resource(){}
		~Resource(){}
	};
	std::shared_ptr<Resource> m_resource;

	struct Instance
	{
		glm::mat4 m_world;
	};
	std::unique_ptr<Instance> m_instance;

	std::unique_ptr<ModelLoader> m_loader;
	static ResourceManager<Resource> s_manager;

public:
	cModel();
	~cModel();

	void load(const std::string& filename);
	void makeInstancing() { ; }
	std::string getFilename()const;
	const cMeshResource* getMesh()const;
	std::shared_ptr<Resource> getResource()const { return m_resource; }
	std::unique_ptr<Instance>& getInstance() { return m_instance; }
};

