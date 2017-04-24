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
#include <btrlib/ThreadPool.h>
#include <btrlib/rTexture.h>
#include <btrlib/ResourceManager.h>

struct TmpCmd
{
	vk::CommandPool m_cmd_pool;
	std::vector<vk::CommandBuffer> m_cmd;
	vk::FenceShared m_fence;
	cDevice m_device;

	vk::CommandBuffer getCmd()const { return m_cmd[0]; }
	vk::FenceShared getFence()const { return m_fence; }
	TmpCmd(const cDevice& device)
	{
		m_device = device;
		{
			// コマンドバッファの準備
			m_cmd_pool = sGlobal::Order().getCmdPoolTempolary(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = 1;
			cmd_buffer_info.commandPool = m_cmd_pool;
			cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
			m_cmd = device->allocateCommandBuffers(cmd_buffer_info);

			// fence
			vk::FenceCreateInfo fence_info;
			m_fence.create(device.getHandle(), fence_info);
		}
		m_cmd[0].reset(vk::CommandBufferResetFlags());
		vk::CommandBufferBeginInfo begin_info;
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
		m_cmd[0].begin(begin_info);
	}
	~TmpCmd()
	{
		m_cmd[0].end();
		vk::SubmitInfo submit_info;
		submit_info.commandBufferCount = (uint32_t)m_cmd.size();
		submit_info.pCommandBuffers = m_cmd.data();

		m_device->getQueue(m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), m_device.getQueueNum(vk::QueueFlagBits::eGraphics) - 1).submit(submit_info, m_fence.getHandle());

		{
			std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
			deleter->pool = m_cmd_pool;
			deleter->cmd = std::move(m_cmd);
			deleter->device = m_device.getHandle();
			deleter->fence_shared.push_back(m_fence);
			sGlobal::Order().destroyResource(std::move(deleter));
		}
	}
};
struct VertexBuffer
{
	struct Zone
	{
		vk::DeviceSize m_start;
		vk::DeviceSize m_end;

		Zone()
			: m_start(0llu)
			, m_end(0llu)
		{}

		Zone split(vk::DeviceSize size)
		{
			Zone newZone;
			newZone.m_start = m_start;
			newZone.m_end = m_start + size;

			m_start += size;

			return newZone;
		}
		void marge(const Zone& zone)
		{
			assert(m_start == zone.m_end);
			m_start = zone.m_start;
		}

		bool tryMarge(const Zone zone)
		{
			if (m_start != zone.m_end)
			{
				return false;
			}
			marge(zone);
			return true;
		}

		vk::DeviceSize range()const { return m_end - m_start; }
		bool isValid()const { return m_end != 0llu; }
	};
	struct FreeZone
	{
		std::vector<Zone> m_free_zone;
		std::vector<Zone> m_active_zone;

		std::mutex m_mutex;
		void setup(vk::DeviceSize size)
		{
			assert(m_free_zone.empty());
			Zone zone;
			zone.m_start = 0;
			zone.m_end = size;

			std::lock_guard<std::mutex> lock(m_mutex);
			m_free_zone.push_back(zone);
		}
		Zone alloc(vk::DeviceSize size) 
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto& zone : m_free_zone)
			{
				if (zone.range() >= size) 
				{
					m_active_zone.emplace_back(zone.split(size));
					return m_active_zone.back();
				}
			}

			// sortしてもう一度
			sort();
			for (auto& zone : m_free_zone)
			{
				if (zone.range() >= size)
				{
					m_active_zone.emplace_back(zone.split(size));
					return m_active_zone.back();
				}
			}
			return Zone();
		}

		void free(const Zone& zone) 
		{
			assert(zone.isValid());

			std::lock_guard<std::mutex> lock(m_mutex);
			{
				// activeから削除
				auto it = std::find_if(m_active_zone.begin(), m_active_zone.end(), [&](Zone& active) { return active.m_start == zone.m_start; });
				m_active_zone.erase(it);
			}

			{
				// freeにマージ
				for (auto& free_zone : m_free_zone)
				{
					if (free_zone.tryMarge(zone)) {
						return;
					}
				}

				// できなければ、新しい領域として追加
				m_free_zone.emplace_back(zone);
			}

		}
	private:
		void sort()
		{
			std::sort(m_free_zone.begin(), m_free_zone.end(), [](Zone& a, Zone& b) { return a.m_start < b.m_start; });
			for (size_t i = m_free_zone.size() - 1; i > 0; i--)
			{
				if (m_free_zone[i - 1].tryMarge(m_free_zone[i])) {
					m_free_zone.erase(m_free_zone.begin() + i);
				}
			}
		}
	};
	struct Resource
	{
		cDevice m_device;
		vk::Buffer m_buffer;

		vk::DeviceMemory m_memory;
		vk::DeviceSize m_allocate_size;
		FreeZone m_free_zone;

		vk::MemoryRequirements m_memory_request;
		vk::MemoryAllocateInfo m_memory_alloc;

		vk::Buffer m_staging_buffer;
		vk::DeviceMemory m_staging_memory;
		char* m_staging_data;

		~Resource()
		{
			m_device->unmapMemory(m_staging_memory);
		}
	};

	struct AllocateBuffer
	{
		vk::DescriptorBufferInfo m_buffer_info;
		struct Resource
		{
			cDevice m_device;
			Zone m_zone;

			vk::Buffer m_staging_buffer_ref;
			char* m_staging_ptr;
		};

		std::shared_ptr<Resource> m_resource;

		template<typename T>
		void update(const std::vector<T>& data, vk::DeviceSize offset)
		{
			size_t data_size = vector_sizeof(data);
			memcpy_s(m_resource->m_staging_ptr + offset, data_size, data.data(), data_size);

			vk::BufferCopy copy_info;
			copy_info.size = data_size;
			copy_info.srcOffset = m_resource->m_zone.m_start + offset;
			copy_info.dstOffset = m_resource->m_zone.m_start + offset;

			TmpCmd tmpcmd(m_resource->m_device);
			auto cmd = tmpcmd.getCmd();
			cmd.copyBuffer(m_resource->m_staging_buffer_ref, m_buffer_info.buffer, copy_info);
		}
		vk::DescriptorBufferInfo getBufferInfo()const { return m_buffer_info; }
		vk::Buffer getBuffer()const { return m_buffer_info.buffer; }
	};

	std::shared_ptr<Resource> m_resource;
	vk::PhysicalDevice m_gpu;

	AllocateBuffer allocate(vk::DeviceSize size)
	{
		auto zone = m_resource->m_free_zone.alloc(size);
		AllocateBuffer alloc;
		auto deleter = [&](AllocateBuffer::Resource* ptr)
		{
			m_resource->m_free_zone.free(ptr->m_zone);
// 			std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
// 			deleter->device = m_resource->m_device.getHandle();
// 			sGlobal::Order().destroyResource(std::move(deleter));
			delete ptr;
		};
 		alloc.m_resource = std::shared_ptr<AllocateBuffer::Resource>(new AllocateBuffer::Resource, deleter);
//		alloc.m_resource = std::make_shared<AllocateBuffer::Resource>();
		alloc.m_resource->m_device = m_resource->m_device;
		alloc.m_resource->m_zone = zone;
		alloc.m_buffer_info.buffer = m_resource->m_buffer;
		alloc.m_buffer_info.offset = alloc.m_resource->m_zone.m_start;
		alloc.m_buffer_info.range = size;
		alloc.m_resource->m_staging_buffer_ref = m_resource->m_staging_buffer;
		alloc.m_resource->m_staging_ptr = m_resource->m_staging_data + alloc.m_resource->m_zone.m_start;
		return alloc;
	}

	VertexBuffer(const cDevice& device, vk::DeviceSize size)
	{
		setup(device, size);
	}
	void setup(const cDevice& device, vk::DeviceSize size)
	{
		m_resource = std::make_shared<Resource>();
		m_gpu = device.getGPU();
		m_resource->m_device = device;

		m_resource->m_free_zone.setup(size);
		vk::BufferCreateInfo buffer_info;
		buffer_info.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
		buffer_info.size = size;
//		buffer_info.flags = vk::BufferCreateFlagBits::eSparseBinding;
		m_resource->m_buffer = device->createBuffer(buffer_info);

		vk::MemoryRequirements memory_request = m_resource->m_device->getBufferMemoryRequirements(m_resource->m_buffer);
		vk::MemoryAllocateInfo memory_alloc;
		memory_alloc.setAllocationSize(memory_request.size);
		memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(m_gpu, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal));
		m_resource->m_memory = m_resource->m_device->allocateMemory(memory_alloc);
		m_resource->m_memory_alloc = memory_alloc;
		m_resource->m_memory_request = memory_request;
		device->bindBufferMemory(m_resource->m_buffer, m_resource->m_memory, 0);

		{
			// host_visibleなバッファの作成
			vk::BufferCreateInfo buffer_info;
			buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
			buffer_info.size = size;

			m_resource->m_staging_buffer = device->createBuffer(buffer_info);

			vk::MemoryRequirements memory_request = device->getBufferMemoryRequirements(m_resource->m_staging_buffer);
			vk::MemoryAllocateInfo memory_alloc;
			memory_alloc.setAllocationSize(memory_request.size);
			memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
			m_resource->m_staging_memory = device->allocateMemory(memory_alloc);
			m_resource->m_staging_data = (char*)device->mapMemory(m_resource->m_staging_memory, 0, buffer_info.size, vk::MemoryMapFlags());

			device->bindBufferMemory(m_resource->m_staging_buffer, m_resource->m_staging_memory, 0);
		}

	}

#if 0
	void setup(const cDevice& device, vk::DeviceSize size)
	{
		m_resource = std::make_shared<Resource>();
		m_gpu = device.getGPU();
		m_resource->m_device = device;

		vk::BufferCreateInfo buffer_info;
		buffer_info.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
		buffer_info.size = size;
		buffer_info.flags = vk::BufferCreateFlagBits::eSparseBinding;
		m_resource->m_buffer = device->createBuffer(buffer_info);

		vk::MemoryRequirements memory_request = m_resource->m_device->getBufferMemoryRequirements(m_resource->m_buffer);
		vk::MemoryAllocateInfo memory_alloc;
		memory_alloc.setAllocationSize(memory_request.size);
		memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(m_gpu, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal));
		m_resource->m_memory = m_resource->m_device->allocateMemory(memory_alloc);
		m_resource->m_memory_alloc = memory_alloc;
		m_resource->m_memory_request = memory_request;
		device->bindBufferMemory(m_resource->m_buffer, m_resource->m_memory, 0);

		//		vk::BufferCreateInfo buffer_info;
		std::vector<vk::Buffer> buffers(10);
		std::vector<vk::DeviceMemory> memories(10);
		std::vector<vk::SparseMemoryBind> sparse_memory_bind(10);
		std::vector<vk::SparseBufferMemoryBindInfo> sparse_bind(10);
		for (int i = 0; i < 10; i++)
		{
			buffer_info.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
			buffer_info.size = 1024;
			buffer_info.flags = vk::BufferCreateFlagBits::eSparseBinding;
			buffers[i] = (device->createBuffer(buffer_info));
			auto request = m_resource->m_device->getBufferMemoryRequirements(buffers[i]);
			vk::MemoryAllocateInfo alloc;
			alloc.setAllocationSize(request.size);
			alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(m_gpu, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal));
			memories[i] = m_resource->m_device->allocateMemory(alloc);
			device->bindBufferMemory(buffers[i], memories[i], 0);

			sparse_memory_bind[i].memory = memories[i];
			sparse_memory_bind[i].memoryOffset = 0;
			sparse_memory_bind[i].size = memory_request.alignment;
			sparse_memory_bind[i].resourceOffset = 0;

			sparse_bind[i].buffer = m_resource->m_buffer;
			sparse_bind[i].bindCount = 1;
			sparse_bind[i].pBinds = &sparse_memory_bind[i];
		}

		bind(sparse_bind);

		for (int i = 0; i < 10; i++)
		{
// 			sparse_memory_bind[i].memory = m_resource->m_memory;
			sparse_memory_bind[i].memoryOffset = 0;
			sparse_memory_bind[i].size = 0;
			sparse_memory_bind[i].resourceOffset = 0;

			sparse_bind[i].buffer = buffers[i];
			sparse_bind[i].bindCount = 1;
			sparse_bind[i].pBinds = &sparse_memory_bind[i];
		}
		bind(sparse_bind);
	}

	void bind(const std::vector<vk::SparseBufferMemoryBindInfo>& sparse_bind)
	{
// 		vk::SparseBufferMemoryBindInfo sparse_bind;
// 		sparse_bind.buffer = m_resource->m_buffer;
// 		sparse_bind.bindCount = sparse_bind_info.size();
// 		sparse_bind.pBinds = sparse_bind_info.data();

		vk::BindSparseInfo bind_sparse_info;
		bind_sparse_info.bufferBindCount = sparse_bind.size();
		bind_sparse_info.pBufferBinds = sparse_bind.data();
		auto queue = m_resource->m_device->getQueue(m_resource->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eSparseBinding), m_resource->m_device.getQueueNum(vk::QueueFlagBits::eSparseBinding) - 1);
		queue.bindSparse(bind_sparse_info, vk::Fence());
		queue.waitIdle();
		int a = 0;
		a++;
	}
	vk::DeviceMemory getSparseMemory()const { return m_resource->m_memory; }
#endif
};
struct cMeshResource 
{

	ConstantBuffer m_indirect_buffer;

	VertexBuffer::AllocateBuffer m_vertex_buffer_ex;
	VertexBuffer::AllocateBuffer m_index_buffer_ex;
	VertexBuffer::AllocateBuffer m_indirect_buffer_ex;
	vk::IndexType mIndexType;
	int32_t mIndirectCount;

	vk::DeviceMemory m_memory;
	vk::Buffer m_buffer;
	std::array<vk::DescriptorBufferInfo, 3> m_buffer_info;


	template<typename... Args>
	void setup(vk::PhysicalDevice gpu, const cDevice& device, vk::BufferUsageFlags flag, Args... data)
	{
		{
			vk::BufferCreateInfo buffer_info;
			buffer_info.usage = flag | vk::BufferUsageFlagBits::eTransferDst;
			buffer_info.size = 
			m_buffer = device->createBuffer(buffer_info);

		}
		m_buffer_info[0].range = vector_sizeof(vertex);
		m_buffer_info[1].offset = m_buffer_info[0].range;
		m_buffer_info[1].range = vector_sizeof(index);
		m_buffer_info[2].offset = m_buffer_info[1].range;
		m_buffer_info[2].range = vector_sizeof(indirect);
		{
			vk::BufferCreateInfo buffer_info;
			buffer_info.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
			buffer_info.size = m_buffer_info[0].range;
			m_vertex_buffer = device->createBuffer(buffer_info);

			buffer_info.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
			buffer_info.size = m_buffer_info[0].range;
			m_index_buffer = device->createBuffer(buffer_info);

			buffer_info.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
			buffer_info.size = m_buffer_info[0].range;
			m_vertex_buffer = device->createBuffer(buffer_info);

		}

		int data_size = m_buffer_info[2].offset + m_buffer_info[2].range;

		vk::BufferCreateInfo staging_buffer_info;
		staging_buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
		staging_buffer_info.size = data_size;
		auto staging_buffer = m_private->m_device.createBuffer(staging_buffer_info);

		vk::MemoryRequirements staging_memory_request = m_private->m_device.getBufferMemoryRequirements(staging_buffer);
		vk::MemoryAllocateInfo staging_memory_alloc;
		staging_memory_alloc.setAllocationSize(staging_memory_request.size);
		staging_memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(m_gpu, staging_memory_request, vk::MemoryPropertyFlagBits::eHostVisible));
		auto staging_memory = m_private->m_device.allocateMemory(staging_memory_alloc);

		vk::CommandPool cmd_pool;
		std::vector<vk::CommandBuffer> cmd;
		{
			// コマンドバッファの準備
			cmd_pool = sGlobal::Order().getCmdPoolTempolary(m_family_index);
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = 1;
			cmd_buffer_info.commandPool = cmd_pool;
			cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
			cmd = m_private->m_device.allocateCommandBuffers(cmd_buffer_info);

		}
		cmd[0].reset(vk::CommandBufferResetFlags());
		vk::CommandBufferBeginInfo begin_info;
		begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit | vk::CommandBufferUsageFlagBits::eSimultaneousUse;
		cmd[0].begin(begin_info);

		char* mem = reinterpret_cast<char*>(m_private->m_device.mapMemory(staging_memory, 0, data_size, vk::MemoryMapFlags()));
		{
			vk::DeviceSize offset = 0;
			void* data[] = {vertex.data(), index.data(), indirect.data()};
			for (size_t i = 0; i < m_buffer_info.size(); i++)
			{
				auto& buffer_info = m_buffer_info[i];
				memcpy_s(mem+offset, m_buffer_info.range, data[i], m_buffer_info.range);

				vk::BufferCopy copy_info;
				copy_info.size = buffer_info.range;
				copy_info.srcOffset = offset;
				copy_info.dstOffset = offset;
				cmd[0].copyBuffer(staging_buffer, buffer_info.buffer, copy_info);

				offset += m_buffer_info.range;
			}
		}



		cmd[0].end();
		vk::SubmitInfo submit_info;
		submit_info.commandBufferCount = (uint32_t)cmd.size();
		submit_info.pCommandBuffers = cmd.data();

		vk::FenceCreateInfo fence_info;
		vk::Fence fence = m_private->m_device.createFence(fence_info);
		m_queue.submit(submit_info, fence);

		{
			std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
			deleter->pool = cmd_pool;
			deleter->cmd = std::move(cmd);
			deleter->device = m_private->m_device;
			deleter->buffer = { staging_buffer };
			deleter->memory = { staging_memory };
			deleter->fence.push_back(fence);
			sGlobal::Order().destroyResource(std::move(deleter));
		}

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
	std::array<ConstantBuffer, MotionBuffer::NUM> mMotionBuffer;

	cAnimation() = default;

	cAnimation(const cAnimation&) = delete;
	cAnimation& operator=(const cAnimation&) = delete;

	cAnimation(cAnimation &&)noexcept = default;
	cAnimation& operator=(cAnimation&&)noexcept = default;

};

struct ResourceTexture
{
	cGPU m_gpu;
	struct Resource 
	{
		std::string m_filename;
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
				deleter->sampler = { m_sampler };
				deleter->memory = { m_memory };
				deleter->fence_shared = { m_fence_shared };
				sGlobal::Order().destroyResource(std::move(deleter));

				m_device->destroyImageView(m_image_view);
			}
		}
	};

	std::shared_ptr<Resource> m_private;

	void load(const cGPU& gpu, const cDevice& device, cThreadPool& thread_pool, const std::string& filename);
	vk::ImageView getImageView()const { return m_private ? m_private->m_image_view : vk::ImageView(); }
	vk::Sampler getSampler()const { return m_private ? m_private->m_sampler : vk::Sampler(); }

	bool isReady()const 
	{
		return m_private ? m_private->m_device->getFenceStatus(*m_private->m_fence_shared) == vk::Result::eSuccess : true;
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
		s32 mInstanceAliveNum;
		s32 mInstanceNum;
		s32 mNodeNum;

		s32 mBoneNum;
		s32 mMeshNum;
		s32 _p[2];

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

public:
	struct Resource
	{
		friend cModel;
	public:
		enum class ModelConstantBuffer : s32
		{
			VERTEX_INDEX_INDIRECT,
			INDIRECT,
			MATERIAL_INDEX,
			MATERIAL,
			VS_MATERIAL,	// vertex stage material
			PLAYING_ANIMATION,
			MOTION_WORK,			//!< ノードがMotionのIndexを
			NODE_INFO,
			//		MESH,
			NODE_LOCAL_TRANSFORM,
			NODE_GLOBAL_TRANSFORM,
			BONE_INFO,
			BONE_TRANSFORM,
			BONE_MAP,	//!< instancingの際のBoneの参照先
			NUM,
		};

		enum class ModelStorageBuffer : s32
		{
			MODEL_INFO,
			WORLD,
			NUM,
		};

		std::array<ConstantBuffer, static_cast<s32>(ModelConstantBuffer::NUM)> m_constant_buffer;
		std::array<StorageBuffer, static_cast<s32>(ModelStorageBuffer::NUM)> m_storage_buffer;
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

		ConstantBuffer m_compute_indirect_buffer;

		const StorageBuffer& getBuffer(ModelStorageBuffer buffer)const { return m_storage_buffer[static_cast<s32>(buffer)]; }
		StorageBuffer& getBuffer(ModelStorageBuffer buffer) { return m_storage_buffer[static_cast<s32>(buffer)]; }
		const ConstantBuffer& getBuffer(ModelConstantBuffer buffer)const { return m_constant_buffer[static_cast<s32>(buffer)]; }
		ConstantBuffer& getBuffer(ModelConstantBuffer buffer) { return m_constant_buffer[static_cast<s32>(buffer)]; }

		const ConstantBuffer& getMotionBuffer(cAnimation::MotionBuffer buffer)const { return m_animation_buffer.mMotionBuffer[buffer]; }
		ConstantBuffer& getMotionBuffer(cAnimation::MotionBuffer buffer) { return m_animation_buffer.mMotionBuffer[buffer]; }

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

