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

struct Descripter
{
	vk::DescriptorSetLayout m_descriptor_set_layout;
	std::vector<vk::DescriptorSet> m_descriptor_set;
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

/**
 * animationデータを格納したテクスチャ
 */
struct MotionTexture
{
	struct Resource
	{
		cDevice m_device;
		vk::FenceShared m_fence_shared;
		vk::Image m_image;
		vk::ImageView m_image_view;
		vk::DeviceMemory m_memory;
		vk::Sampler m_sampler;
		~Resource()
		{
			if (m_image)
			{
				std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
				deleter->device = m_device.getHandle();
				deleter->image = { m_image };
//				deleter->sampler = { m_sampler };
				deleter->memory = { m_memory };
				deleter->fence_shared = { m_fence_shared };
				sGlobal::Order().destroyResource(std::move(deleter));

				m_device->destroyImageView(m_image_view);
			}
		}
	};

	std::shared_ptr<Resource> m_resource;

//	void create(const cDevice& device, );
	vk::ImageView getImageView()const { return m_resource ? m_resource->m_image_view : vk::ImageView(); }

	bool isReady()const
	{
		return m_resource ? m_resource->m_device->getFenceStatus(*m_resource->m_fence_shared) == vk::Result::eSuccess : true;
	}
};

struct cAnimation
{
	enum MotionBuffer : s32
	{
		ANIMATION_INFO,
		NUM,
	};
	std::array<btr::AllocatedMemory, MotionBuffer::NUM> mMotionBuffer;

	cAnimation() = default;

	cAnimation(const cAnimation&) = delete;
	cAnimation& operator=(const cAnimation&) = delete;

	cAnimation(cAnimation &&)noexcept = default;
	cAnimation& operator=(cAnimation&&)noexcept = default;

	MotionTexture m_motion_texture;
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

class sModel : Singleton<sModel>
{
	friend Singleton<sModel>;

	btr::BufferMemory m_vertex_index;
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

	struct MaterialBuffer {
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		glm::vec4		mEmissive;
		std::uint64_t	mDiffuseTex;
		std::uint64_t	mAmbientTex;
		std::uint64_t	mSpecularTex;
		std::uint64_t	__pp;
		float			mShininess;
		float			__p;
		float			__p1;
		float			__p2;
	};
	struct AnimationInfo
	{
		//	int animationNo_;
		float maxTime_;
		float ticksPerSecond_;
		int numInfo_;
		int offsetInfo_;
	};
	struct PlayingAnimation
	{
		int		playingAnimationNo;	//!< モーション
		float	time;				//!< モーションの再生時間 
		int		currentMotionInfoIndex;	//!< 今再生しているモーションデータのインデックス
		int		isLoop;				//!<
	};

	struct BoneInfo
	{
		s32 mNodeIndex;
		s32 _p[3];
		glm::mat4 mBoneOffset;
	};

	struct Mesh
	{
		vk::DrawIndexedIndirectCommand m_draw_cmd;
		s32 mNumIndex;
		s32 mNumVertex;
		s32 mNodeIndex;	//!< 
		glm::vec4 mAABB;	// xyz: center w:radius

		Mesh()
			: mNumIndex(0)
			, mNumVertex(0)
			, mNodeIndex(-1)
		{}

		Mesh(const vk::DrawIndexedIndirectCommand& cmd)
			: Mesh()
		{
			m_draw_cmd = cmd;
		}

		void set(const std::vector<glm::vec3>& v, const std::vector<unsigned>& i);
	};
	struct ModelInfo {
		s32 mInstanceMaxNum;		//!< 出せるモデルの量
		s32 mInstanceAliveNum;		//!< 生きているモデルの量
		s32 mInstanceNum;			//!< 実際に描画する数
		s32 mNodeNum;

		s32 mBoneNum;
		s32 mMeshNum;
		s32 m_node_depth_max;
		s32 _p[1];

		glm::vec4 mAabb;
		glm::mat4 mInvGlobalMatrix;
	};
	struct NodeInfo {
		int32_t		mNodeNo;
		int32_t		mParent;
		int32_t		mBoneIndex;
		int32_t		m_depth;	//!< RootNodeからの深さ

		NodeInfo()
			: mNodeNo(-1)
			, mParent(-1)
			, mBoneIndex(-1)
			, m_depth(0)
		{
		}
	};

	struct NodeTransformBuffer {
		glm::mat4	localAnimated_;		//!< parentMatrix,Worldをかけていない行列
		glm::mat4	globalAnimated_;
	};
	struct NodeLocalTransformBuffer {
		glm::mat4	localAnimated_;		//!< parentMatrix,Worldをかけていない行列
	};
	struct NodeGlobalTransformBuffer {
		//	glm::mat3x4	globalAnimated_;
		glm::mat4	globalAnimated_;
	};

	struct BoneTransformBuffer {
		glm::mat4	value_;
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

public:
	struct Resource
	{
		friend cModel;
	public:

		enum class ModelStorageBuffer : s32
		{
			MODEL_INFO,
			NODE_INFO,
			BONE_INFO,
			PLAYING_ANIMATION,
			MATERIAL_INDEX,
			MATERIAL,
			VS_MATERIAL,	// vertex stage material
			NODE_LOCAL_TRANSFORM,
			NODE_GLOBAL_TRANSFORM,
			BONE_TRANSFORM,
			BONE_MAP,	//!< instancingの際のBoneの参照先
			WORLD,
			NUM,
		};

		btr::BufferMemory m_storage_memory;
		btr::BufferMemory m_uniform_memory;
		btr::BufferMemory m_storage_uniform_memory;
		std::array<btr::AllocatedMemory, static_cast<s32>(ModelStorageBuffer::NUM)> m_storage_buffer;

		btr::AllocatedMemory m_world_staging_buffer;
		btr::AllocatedMemory m_model_info_staging_buffer;

		cAnimation m_animation_buffer;

		std::string m_filename;
		ModelInfo m_model_info;
		std::vector<int> mIndexNum;
		std::vector<int> mVertexNum;
		int				mMeshNum;

		std::vector<Material>	m_material;
		std::vector<int>		m_material_index;


		cMeshResource mMesh;
		RootNode mNodeRoot;
		std::vector<Bone> mBone;

		btr::AllocatedMemory m_compute_indirect_buffer;

		const btr::AllocatedMemory& getBuffer(ModelStorageBuffer buffer)const { return m_storage_buffer[static_cast<s32>(buffer)]; }
		btr::AllocatedMemory& getBuffer(ModelStorageBuffer buffer) { return m_storage_buffer[static_cast<s32>(buffer)]; }

		const btr::AllocatedMemory& getMotionBuffer(cAnimation::MotionBuffer buffer)const { return m_animation_buffer.mMotionBuffer[buffer]; }
		btr::AllocatedMemory& getMotionBuffer(cAnimation::MotionBuffer buffer) { return m_animation_buffer.mMotionBuffer[buffer]; }

		cAnimation& getAnimation() { return m_animation_buffer; }

		Resource()
		{

		}
		~Resource() 
		{
		}
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

	std::string getFilename()const;
	const cMeshResource* getMesh()const;
	std::shared_ptr<Resource> getResource()const { return m_resource; }
	std::unique_ptr<Instance>& getInstance() { return m_instance; }
};

