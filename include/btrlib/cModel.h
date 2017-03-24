#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <future>

#include <btrlib/Define.h>
#include <btrlib/sVulkan.h>
#include <btrlib/ThreadPool.h>
#include <btrlib/rTexture.h>
struct cMeshGPU {
	vk::Buffer mBuffer;
	vk::DeviceMemory mMemory;
	std::vector<vk::VertexInputBindingDescription> mBinding;
	std::vector<vk::VertexInputAttributeDescription> mAttribute;

	vk::IndexType mIndexType;
	int mIndirectCount;
	int mBufferSize[3];

	vk::Buffer mBufferIndirect;
	vk::DeviceMemory mMemoryIndirect;
	vk::DescriptorBufferInfo mIndirectInfo;
	// 削除予定
	vk::PipelineVertexInputStateCreateInfo mVertexInfo;
};

class  BufferBase : public GPUResource
{
	virtual bool isEnd() const override
	{
		return m_device->getFenceStatus(m_fence) == vk::Result::eSuccess;
	}

public:
	cDevice m_device;

	vk::Buffer m_buffer;
	vk::DeviceMemory m_memory;
	vk::Semaphore m_semaphore;
	vk::Fence m_fence;
	vk::Event m_event;
	vk::DescriptorBufferInfo m_buffer_info;
	vk::MemoryRequirements m_memory_request;

protected:
	BufferBase() = default;
	~BufferBase()
	{
		if (!m_buffer)
		{
			m_device->destroyBuffer(m_buffer);
			m_buffer = VK_NULL_HANDLE;
		}

		if (!m_memory)
		{
			m_device->freeMemory(m_memory);
			m_memory = VK_NULL_HANDLE;
		}

		if (!m_fence)
		{
			m_device->destroyFence(m_fence);
			m_fence = VK_NULL_HANDLE;
		}

		if (!m_semaphore)
		{
			m_device->destroySemaphore(m_semaphore);
			m_semaphore = VK_NULL_HANDLE;
		}

		if (!m_event)
		{
			m_device->destroyEvent(m_event);
			m_event = VK_NULL_HANDLE;
		}
	}

protected:
	void createBuffer(const cDevice& device, vk::BufferCreateInfo bufferInfo, vk::MemoryAllocateInfo mem_alloc)
	{
		m_device = device;

		m_buffer = device->createBuffer(bufferInfo);

		m_memory_request = device->getBufferMemoryRequirements(m_buffer);

		mem_alloc.setAllocationSize(m_memory_request.size);
		m_memory = device->allocateMemory(mem_alloc);
		device->bindBufferMemory(m_buffer, m_memory, 0);

		m_buffer_info
			.setBuffer(m_buffer)
			.setOffset(0)
			.setRange(m_memory_request.size);

		vk::SemaphoreCreateInfo info;
		m_semaphore = device->createSemaphore(info);

		vk::FenceCreateInfo fence_info = vk::FenceCreateInfo().setFlags(vk::FenceCreateFlagBits::eSignaled);
		m_fence = m_device->createFence(fence_info);
		vk::EventCreateInfo event_info = vk::EventCreateInfo();
		m_event = m_device->createEvent(event_info);
	}

};
struct StagingBuffer : public BufferBase
{
	void allocate(const cDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage)
	{
		vk::BufferCreateInfo buffer_info = vk::BufferCreateInfo()
			.setUsage(usage)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);

		vk::MemoryAllocateInfo mem_alloc = vk::MemoryAllocateInfo()
			.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(device.getGPU(), m_memory_request, vk::MemoryPropertyFlagBits::eHostVisible));

		createBuffer(device, buffer_info, mem_alloc);
	}

	template<typename T>
	void insert(const std::vector<T>& data, vk::DeviceSize offset)
	{
		void* mem = m_device->mapMemory(m_memory, 0, m_memory_request.size, vk::MemoryMapFlags());
		{
			size_t size = vector_sizeof(data);
			memcpy_s(mem, size, data.data(), size);
		}
		m_device->unmapMemory(m_memory);
	}


	std::unique_ptr<std::vector<char*>> get()const
	{
		std::unique_ptr<std::vector<char*>> ptr = std::make_unique<std::vector<char*>>();
		void* mem = m_device->mapMemory(m_memory, 0, m_memory_request.size, vk::MemoryMapFlags());
		{
			ptr->resize(m_memory_request.size);
			memcpy_s(ptr->data(), m_memory_request.size, mem, m_memory_request.size);
		}
		m_device->unmapMemory(m_memory);
		return ptr;
	}


};
//!< 一度初期化すると読み取りのみ行うバッファ
struct DataResourceBuffer : public BufferBase
{
	void allocate(const cDevice& device, vk::DeviceSize size, vk::BufferUsageFlags usage)
	{
		vk::BufferCreateInfo buffer_info = vk::BufferCreateInfo()
			.setUsage(usage | vk::BufferUsageFlagBits::eTransferDst)
			.setSize(size)
			.setSharingMode(vk::SharingMode::eExclusive);

		vk::MemoryAllocateInfo mem_alloc = vk::MemoryAllocateInfo()
			.setMemoryTypeIndex(
				cGPU::Helper::getMemoryTypeIndex(device.getGPU(), m_memory_request,
				vk::MemoryPropertyFlagBits::eDeviceLocal)
			);
		createBuffer(device, buffer_info, mem_alloc);
	}

	// コピーのキューを積む
	template<typename T>
	void copyFrom(const std::vector<T>& src, vk::DeviceSize offset, vk::CommandBuffer cmd)
	{
		StagingBuffer* staging = GPUResource::Manager::Order().create<StagingBuffer>();
		auto size = vector_sizeof(src);
		staging->allocate(m_device, size, vk::BufferUsageFlagBits::eTransferSrc);
		staging->insert(src, 0);

		vk::BufferCopy copy_info = vk::BufferCopy()
			.setDstOffset(offset)
			.setSize(size);
		cmd.copyBuffer(staging->m_buffer, m_buffer, copy_info);
		GPUResource::Manager::Order().destroy(staging);

	}

	void copyFrom(const BufferBase& src, const vk::BufferCopy& copy_info, vk::CommandBuffer cmd)
	{
		cmd.copyBuffer(src.m_buffer, m_buffer, copy_info);
	}

	template<typename T>
	std::future<std::unique_ptr<std::vector<T>>> getAsync(cDevice& device, cThreadPool& thread_pool)const
	{
		// ふつうは使わない。デバッグ用
		// DeviceLocalには直接アクセスできないので、いったんHostVisibleにコピーしてから取得する
		auto task = std::make_shared<std::promise<std::unique_ptr<std::vector<T>>>>();
		auto future = task->get_future();
		{
			ThreadJob job;
			StagingBuffer* staging = nullptr;
			vk::FenceCreateInfo fence_info = vk::FenceCreateInfo()
				//.setFlags(/*vk::FenceCreateFlagBits::*/)
				;
			vk::Fence fence; //= device->createFence(fence_info);
			auto setup = [=]() mutable
			{
				staging = GPUResource::Manager::Order().create<StagingBuffer>();
				staging->allocate(device, m_memory_request.size, vk::BufferUsageFlagBits::eTransferDst);

				vk::BufferCopy copy_info = vk::BufferCopy()
					.setSize(m_memory_request.size);
				cmd.copyBuffer(m_buffer, staging->m_buffer, copy_info);

				vk::SubmitInfo submit_info = vk::SubmitInfo();
				device->getQueue(device.getQueueFamilyIndex(), 0).submit(submit_info, fence);
			};
			job.mJob.push_back(setup);

			std::function<void()> wait_loop = [=, &thread_pool, & wait_loop]() mutable
			{
				if (vk::Result::eSuccess == device->getFenceStatus(fence))
				{
					std::unique_ptr<std::vector<T>> ptr = std::make_unique<std::vector<T>>();
					void* mem = m_device->mapMemory(m_memory, 0, m_memory_request.size, vk::MemoryMapFlags());
					{
						// todo aliment
						ptr->resize(m_memory_request.size / sizeof(T));
						memcpy_s(ptr->data(), m_memory_request.size, mem, m_memory_request.size);
					}
					m_device->unmapMemory(m_memory);
					m_device->destroyFence(fence);

					GPUResource::Manager::Order().destroy(staging);

					task->set_value(std::move(ptr));
				}
				else
				{
					// まだ終わっていないのでさらに待つ
					cThreadJob wait_job;
					wait_job.mFinish = wait_loop;
					thread_pool.enque(wait_job);
				}

			};
			job.mFinish = wait_loop;
			thread_pool.enque(job);
		}
		return std::move(future);
	}

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
	std::array<UniformBuffer, MotionBuffer::NUM> mMotionBuffer;
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
		int			mNodeNo;
		int			mParent;
		int			mBoneIndex;
		int			mNodeName;

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
		glm::mat3x4	localAnimated_;		//!< parentMatrix,Worldをかけていない行列
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


		std::array<UniformBuffer, static_cast<s32>(ModelBuffer::NUM)> mUniformBuffer;
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

		const UniformBuffer& getBuffer(ModelBuffer buffer)const { return mUniformBuffer[static_cast<s32>(buffer)]; }
		UniformBuffer& getBuffer(ModelBuffer buffer) { return mUniformBuffer[static_cast<s32>(buffer)]; }

		const UniformBuffer& getMotionBuffer(cAnimation::MotionBuffer buffer)const { return m_animation_buffer.mMotionBuffer[buffer]; }
		UniformBuffer& getMotionBuffer(cAnimation::MotionBuffer buffer) { return m_animation_buffer.mMotionBuffer[buffer]; }

		Private()
		{

		}
		~Private() {
			// @ ToDo bufferのdelete
			;
		}

		const vk::PipelineVertexInputStateCreateInfo& getPipelineVertexInput()const {
			return mMesh.mVertexInfo;
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

