#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <future>

#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/ThreadPool.h>
#include <btrlib/rTexture.h>
struct cMeshGPU {
	vk::Buffer mBuffer;
	vk::DeviceMemory mMemory;

	vk::IndexType mIndexType;
	int32_t mIndirectCount;
	int32_t mBufferSize[3];

	vk::Buffer mBufferIndirect;
	vk::DeviceMemory mMemoryIndirect;
	vk::DescriptorBufferInfo mIndirectInfo;

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
// 	~RootNode() {
// 		int a = 0;
// 	}
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
struct cAnimation
{
	enum MotionBuffer : s32
	{
		ANIMATION_INFO,
		MOTION_INFO,
		MOTION_DATA_TIME,
		MOTION_DATA_SRT,
		NUM,
	};
	std::array<ConstantBuffer, MotionBuffer::NUM> mMotionBuffer;

	cAnimation() = default;

	cAnimation(const cAnimation&) = delete;
	cAnimation& operator=(const cAnimation&) = delete;

	cAnimation(cAnimation &&)noexcept = default;
	cAnimation& operator=(cAnimation&&)noexcept = default;

};
class cModel
{
protected:

public:
	struct Vertex {
		enum {
			BONE_NUM = 4,
		};
		glm::vec4 m_position;
		glm::vec4 m_normal;
		glm::vec4 m_texcoord0;
		int			m_bone_ID[BONE_NUM];
		float		m_weight[BONE_NUM];
		Vertex()
		{
			for (size_t i = 0; i < BONE_NUM; i++) {
				m_bone_ID[i] = -1;
				m_weight[i] = 0.f;
			}
		}
	};

	struct Texture
	{
		vk::Fence m_fence;

		vk::Image m_image;
		vk::ImageView m_image_view;
		vk::DeviceMemory m_memory;

		vk::Sampler m_sampler;
	};
	struct Material {
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		Texture			mDiffuseTex;
		Texture			mAmbientTex;
		Texture			mNormalTex;
		Texture			mSpecularTex;
		Texture			mHeightTex;
		Texture			mReflectionTex;
		float			mShininess;
		glm::vec4		mEmissive;
	};

	struct VSMaterialBuffer {
		std::uint64_t	normalTex_;
		std::uint64_t	heightTex_;
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
	struct MotionInfo
	{
		unsigned mNodeNo;
		int offsetData_;
		int numData_;
		int _p;
	};
	struct MotionBuffer
	{
		float time_;
		float _p1, _p2, _p3;
		glm::vec3 pos_;
		float _pp;
		glm::vec3 scale_;
		float _pp2;
		glm::quat rot_;
	};

	struct MotionTimeBuffer {
		float time_;
	};
	struct MotionDataBuffer
	{
		glm::vec4 posAndScale_;	//!< xyz : pos, w : scale
		glm::quat rot_;
	};

	struct BoneInfo
	{
		s32 mNodeIndex;
		s32 _p[3];
		glm::mat4 mBoneOffset;
	};

	struct MotionWork
	{
		s32 motionInfoIndex;	//!< MotionInfoの場所を計算して保存しておく
		s32 motionBufferIndex;	//!< MotionBufferの場所を計算して保存しておく（毎回０から捜査するのは遅いから）
		s32 _p[2];
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
		s32 mInstanceMaxNum;
		s32 mInstanceNum;
		s32 mNodeNum;
		s32 mBoneNum;

		s32 mMeshNum;
		s32 _p[3];

		glm::vec4 mAabb;
		glm::mat4 mInvGlobalMatrix;
	};
	struct NodeInfo {
		enum {
			SUBMESH_NUM = 999,
			CHILD_NUM = 999,
		};
		int32_t		mNodeNo;
		int32_t		mParent;
		int32_t		mBoneIndex;
		int32_t		mNodeName;

		NodeInfo()
			: mNodeNo(-1)
			, mParent(-1)
			, mBoneIndex(-1)
			, mNodeName(0)
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


	class Private
	{
		friend cModel;
	public:
		enum class ModelBuffer : s32
		{
			VERTEX_INDEX_INDIRECT,
			INDIRECT,
			MATERIAL_INDEX,
			MATERIAL,
			VS_MATERIAL,	// vertex stage material
			PLAYING_ANIMATION,
			MOTION_WORK,			//!< ノードがMotionのIndexを
			MODEL_INFO,
			NODE_INFO,
			//		MESH,
			NODE_LOCAL_TRANSFORM,
			NODE_GLOBAL_TRANSFORM,
			BONE_INFO,
			BONE_TRANSFORM,
			BONE_MAP,	//!< instancingの際のBoneの参照先
			WORLD,
			NUM,
		};


		std::array<ConstantBuffer, static_cast<s32>(ModelBuffer::NUM)> mUniformBuffer;
		cAnimation m_animation_buffer;

		std::string mFilename;
		std::vector<int> mIndexNum;
		std::vector<int> mVertexNum;
		int				mMeshNum;

		std::vector<Material>	m_material;
		std::vector<int>		m_material_index;


		cMeshGPU mMesh;
		RootNode mNodeRoot;
		std::vector<Bone> mBone;

		vk::Buffer m_compute_indirect_buffer;
		vk::DeviceMemory m_compute_indirect_memory;
		vk::DescriptorBufferInfo m_compute_indirect_buffer_info;

		const ConstantBuffer& getBuffer(ModelBuffer buffer)const { return mUniformBuffer[static_cast<s32>(buffer)]; }
		ConstantBuffer& getBuffer(ModelBuffer buffer) { return mUniformBuffer[static_cast<s32>(buffer)]; }

		const ConstantBuffer& getMotionBuffer(cAnimation::MotionBuffer buffer)const { return m_animation_buffer.mMotionBuffer[buffer]; }
		ConstantBuffer& getMotionBuffer(cAnimation::MotionBuffer buffer) { return m_animation_buffer.mMotionBuffer[buffer]; }

		Private()
		{

		}
		~Private() {
			// @ ToDo bufferのdelete
			;
		}
	};
	std::shared_ptr<Private> mPrivate;

	// @ToDo どっかへ移動
	unsigned frustomBO_;

public:
	cModel();
	~cModel();

	void load(const std::string& filename);

	const std::string& getFilename()const;
	const cMeshGPU& getMesh()const;

	std::vector<vk::CommandBuffer> m_cmd_graphics;
	std::vector<vk::CommandBuffer> m_cmd_compute;
};

