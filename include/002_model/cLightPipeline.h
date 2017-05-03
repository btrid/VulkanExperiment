#pragma once
#include <array>
#include <memory>
#include <mutex>
#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/BufferMemory.h>

struct cModelRenderer;

struct StagingBuffer
{
	btr::AllocatedMemory m_device_memory;
	btr::AllocatedMemory m_staging_memory;
	void setup(btr::BufferMemory staging_memory, vk::DeviceSize size)
	{
		m_staging_memory = staging_memory.allocateMemory(size);
	}

	template<typename T>
	T* getPtr(vk::DeviceSize offset) { return reinterpret_cast<T*>(m_staging_memory.getMappedPtr()) + offset; }
	vk::DeviceSize getOffset()const { return m_staging_memory.getOffset(); }
	vk::DeviceSize getSize()const { return m_staging_memory.getSize(); }
	vk::Buffer getBuffer()const { return m_staging_memory.getBuffer(); }
	vk::DeviceMemory getMemory()const { return m_staging_memory.getDeviceMemory(); }
};
struct LightParam {
	glm::vec4 m_position;
	glm::vec4 m_emission;
};

struct FrustomPoint
{
	glm::vec4 ltn;	//!< nearTopLeft
	glm::vec4 rtn;
	glm::vec4 lbn;
	glm::vec4 rbn;
	glm::vec4 ltf;	//!< nearTopLeft
	glm::vec4 rtf;
	glm::vec4 lbf;
	glm::vec4 rbf;
};

struct ComputePipeline
{
	vk::Pipeline					m_pipeline;
	vk::PipelineLayout				m_pipeline_layout;
	vk::DescriptorSetLayout			m_descriptor_set_layout;

	vk::PipelineShaderStageCreateInfo	m_shader_info;

};
struct Light
{
	virtual bool update()
	{
		return true;
	}

	virtual LightParam getParam()const
	{
		return LightParam();
	}
};
struct Frustom2
{
	Plane p[4];
};


struct cFowardPlusPipeline
{
	struct LightInfo
	{
		glm::uvec2 m_resolution;
		glm::uvec2 m_tile_size;

		glm::uvec2 m_tile_num;
		uint32_t m_active_light_num;
		uint32_t _p;
	};
	enum
	{
		COMPUTE_MAKE_FRUSTOM,
		//			COMPUTE_MAKE_LIGHT,
		COMPUTE_CULL_LIGHT,
		COMPUTE_NUM,
	};

	vk::DescriptorBufferInfo getLightInfoBufferInfo()const { return m_private->m_light_info_gpu.getBufferInfo(); }
	vk::DescriptorBufferInfo getLightBufferInfo()const { return m_private->m_light.getBufferInfo(); }
	vk::DescriptorBufferInfo getLightLLHeadBufferInfo()const { return m_private->m_lightLL_head.getBufferInfo(); }
	vk::DescriptorBufferInfo getLightLLBufferInfo()const { return m_private->m_lightLL.getBufferInfo(); }

	struct Private
	{
		cDevice m_device;
		LightInfo m_light_info;

		btr::BufferMemory m_uniform_memory;
		btr::BufferMemory m_storage_memory;
		btr::BufferMemory m_staging_memory;

		btr::UpdateBuffer<LightInfo> m_light_info_gpu;
		btr::UpdateBuffer<FrustomPoint> m_frustom_point;

		btr::AllocatedMemory m_light;
		btr::AllocatedMemory m_lightLL_head;
		btr::AllocatedMemory m_lightLL;
		btr::AllocatedMemory m_tiled_frustom;
		btr::AllocatedMemory m_light_counter;

		StagingBuffer m_light_cpu;

		uint32_t m_light_num;

		std::vector<std::unique_ptr<Light>> m_light_list;
		std::vector<std::unique_ptr<Light>> m_light_list_new;
		std::mutex m_light_new_mutex;

		vk::DescriptorPool m_descriptor_pool;
		vk::PipelineCache m_cache;
		std::array<vk::Pipeline, COMPUTE_NUM> m_pipeline;
		std::array<vk::PipelineLayout, COMPUTE_NUM> m_pipeline_layout;
		std::array<vk::DescriptorSetLayout, COMPUTE_NUM> m_descriptor_set_layout;
		std::vector<vk::DescriptorSet> m_compute_descriptor_set;

		std::array<vk::PipelineShaderStageCreateInfo, COMPUTE_NUM> m_shader_info;
		std::array<vk::PipelineShaderStageCreateInfo, 2> m_shader_info_debug;

		std::array<ComputePipeline, COMPUTE_NUM> m_pipeline_ex;

		void setup(cModelRenderer& renderer);
	};
	std::unique_ptr<Private> m_private;

	void setup(cModelRenderer& renderer)
	{
		m_private = std::make_unique<Private>();
		m_private->setup(renderer);
	}

	void add(std::unique_ptr<Light>&& light)
	{
		m_private->m_light_list_new.push_back(std::move(light));
	}

	void execute(vk::CommandBuffer cmd)
	{
		{
			vk::BufferMemoryBarrier to_copy_barrier;
//			to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_copy_barrier.buffer = m_private->m_light_counter.getBufferInfo().buffer;
			to_copy_barrier.setOffset(m_private->m_light_counter.getBufferInfo().offset);
			to_copy_barrier.setSize(m_private->m_light_counter.getBufferInfo().range);
			to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			uint32_t zero = 0;
			cmd.updateBuffer<uint32_t>(m_private->m_light_counter.getBuffer(), m_private->m_light_counter.getOffset(), { zero });

			vk::BufferMemoryBarrier to_shader_read_barrier;
//			to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.buffer = m_private->m_light_counter.getBufferInfo().buffer;
			to_shader_read_barrier.setOffset(m_private->m_light_counter.getBufferInfo().offset);
			to_shader_read_barrier.setSize(m_private->m_light_counter.getBufferInfo().range);
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}
		{
			auto* camera = cCamera::sCamera::Order().getCameraList()[0];
			Frustom frustom;
			frustom.setup(*camera);
			FrustomPoint frustom_point;
			frustom_point.ltn = glm::vec4(frustom.ltn_, 1.f);
			frustom_point.lbn = glm::vec4(frustom.lbn_, 1.f);
			frustom_point.rtn = glm::vec4(frustom.rtn_, 1.f);
			frustom_point.rbn = glm::vec4(frustom.rbn_, 1.f);
			frustom_point.ltf = glm::vec4(frustom.ltf_, 1.f);
			frustom_point.lbf = glm::vec4(frustom.lbf_, 1.f);
			frustom_point.rtf = glm::vec4(frustom.rtf_, 1.f);
			frustom_point.rbf = glm::vec4(frustom.rbf_, 1.f);

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_copy_barrier.buffer = m_private->m_frustom_point.getBufferInfo().buffer;
			to_copy_barrier.setOffset(m_private->m_frustom_point.getBufferInfo().offset);
			to_copy_barrier.setSize(m_private->m_frustom_point.getBufferInfo().range);
			to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			m_private->m_frustom_point.subupdate(frustom_point);
			m_private->m_frustom_point.update(cmd);

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.buffer = m_private->m_frustom_point.getBufferInfo().buffer;
			to_shader_read_barrier.setOffset(m_private->m_frustom_point.getBufferInfo().offset);
			to_shader_read_barrier.setSize(m_private->m_frustom_point.getBufferInfo().range);
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_private->m_pipeline[COMPUTE_MAKE_FRUSTOM]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_private->m_pipeline_layout[COMPUTE_MAKE_FRUSTOM], 0, m_private->m_compute_descriptor_set[COMPUTE_MAKE_FRUSTOM], {});
			cmd.dispatch(20, 20, 1);
		}

		// light�̍X�V
		{
			// �V�������C�g�̒ǉ�
			std::vector<std::unique_ptr<Light>> light;
			{
				std::lock_guard<std::mutex> lock(m_private->m_light_new_mutex);
				light = std::move(m_private->m_light_list_new);
			}
			m_private->m_light_list.insert(m_private->m_light_list.end(), std::make_move_iterator(light.begin()), std::make_move_iterator(light.end()));
		}

		// �X�V�Ǝ����̐s�������C�g�̍폜
		m_private->m_light_list.erase(std::remove_if(m_private->m_light_list.begin(), m_private->m_light_list.end(), [](auto& p) { return !p->update(); }), m_private->m_light_list.end());

		{
			// ���C�g�̃f�[�^��device�ɃR�s�[
			size_t frame_offset = sGlobal::Order().getCurrentFrame() * m_private->m_light_num;
			auto* p = m_private->m_light_cpu.getPtr<LightParam>(frame_offset);
			uint32_t index = 0;
			for (auto& it : m_private->m_light_list)
			{
				assert(index < m_private->m_light_num);
				p[index] = it->getParam();
				index++;
			}
			{
				vk::BufferMemoryBarrier to_copy_barrier;
				to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_copy_barrier.buffer = m_private->m_light_info_gpu.getBufferInfo().buffer;
				to_copy_barrier.setOffset(m_private->m_light_info_gpu.getBufferInfo().offset);
				to_copy_barrier.setSize(m_private->m_light_info_gpu.getBufferInfo().range);
				to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

				vk::BufferMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read_barrier.buffer = m_private->m_light_info_gpu.getBufferInfo().buffer;
				to_shader_read_barrier.setOffset(m_private->m_light_info_gpu.getBufferInfo().offset);
				to_shader_read_barrier.setSize(m_private->m_light_info_gpu.getBufferInfo().range);
				to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

				m_private->m_light_info.m_active_light_num = index;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
				m_private->m_light_info_gpu.subupdate(m_private->m_light_info);
				m_private->m_light_info_gpu.update(cmd);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
			}

			vk::BufferCopy copy_info;
			copy_info.size = index * sizeof(LightParam);
			copy_info.srcOffset = m_private->m_light_cpu.getOffset() + frame_offset * sizeof(LightParam);
			copy_info.dstOffset = m_private->m_light.getOffset();

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_copy_barrier.buffer = m_private->m_light.getBuffer();
			to_copy_barrier.setOffset(m_private->m_light.getOffset());
			to_copy_barrier.setSize(copy_info.size);
			to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.buffer = m_private->m_light.getBuffer();
			to_shader_read_barrier.setOffset(m_private->m_light.getOffset());
			to_shader_read_barrier.setSize(copy_info.size);
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
			cmd.copyBuffer(m_private->m_light_cpu.getBuffer(), m_private->m_light.getBuffer(), copy_info);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}

		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_private->m_pipeline[COMPUTE_CULL_LIGHT]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_private->m_pipeline_layout[COMPUTE_CULL_LIGHT], 0, m_private->m_compute_descriptor_set[COMPUTE_CULL_LIGHT], {});
			cmd.dispatch(20, 20, 1);

/*			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_copy_barrier.buffer = m_private->m_light.getBuffer();
			to_copy_barrier.setOffset(m_private->m_light.getOffset());
			to_copy_barrier.setSize(copy_info.size);
			to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
*/
			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.buffer = m_private->m_lightLL_head.getBuffer();
			to_shader_read_barrier.setOffset(m_private->m_lightLL_head.getOffset());
			to_shader_read_barrier.setSize(m_private->m_lightLL_head.getSize());
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			vk::BufferMemoryBarrier to_shader_read_barrier2;
			to_shader_read_barrier2.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier2.buffer = m_private->m_lightLL.getBuffer();
			to_shader_read_barrier2.setOffset(m_private->m_lightLL.getOffset());
			to_shader_read_barrier2.setSize(m_private->m_lightLL.getSize());
			to_shader_read_barrier2.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
			to_shader_read_barrier2.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, { to_shader_read_barrier, to_shader_read_barrier2 }, {});
		}

	}

	void executeDebug(vk::CommandBuffer cmd)
	{

	}

};

