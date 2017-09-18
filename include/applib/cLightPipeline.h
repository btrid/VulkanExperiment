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
#include <btrlib/Light.h>

struct cModelInstancingRenderer;

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
struct LightLL
{
	uint32_t next;
	uint32_t light_index;
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

	enum ShaderModule
	{
		//			COMPUTE_MAKE_LIGHT,
		SHADER_COMPUTE_CULL_LIGHT,
		SHADER_NUM,
	};

	enum
	{
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
		btr::AllocatedMemory m_light_counter;

		btr::StagingBuffer m_light_cpu;

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

		std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
		std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
		std::array<vk::PipelineShaderStageCreateInfo, 2> m_shader_info_debug;

		std::array<ComputePipeline, COMPUTE_NUM> m_pipeline_ex;

		void setup(cModelInstancingRenderer& renderer);
	};
	std::unique_ptr<Private> m_private;

	void setup(cModelInstancingRenderer& renderer)
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
			{
				auto to_copy_barrier = m_private->m_light_counter.makeMemoryBarrierEx();
				to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
			}

			uint32_t zero = 0;
			cmd.updateBuffer<uint32_t>(m_private->m_light_counter.getBufferInfo().buffer, m_private->m_light_counter.getBufferInfo().offset, { zero });

			auto to_shader_read_barrier = m_private->m_light_counter.makeMemoryBarrierEx();
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite);
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

			auto to_copy_barrier = m_private->m_frustom_point.getAllocateMemory().makeMemoryBarrierEx();
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			m_private->m_frustom_point.subupdate(frustom_point);
			m_private->m_frustom_point.update(cmd);

			auto to_shader_read_barrier = m_private->m_frustom_point.getAllocateMemory().makeMemoryBarrierEx();
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}

		// lightの更新
		{
			// 新しいライトの追加
			std::vector<std::unique_ptr<Light>> light;
			{
				std::lock_guard<std::mutex> lock(m_private->m_light_new_mutex);
				light = std::move(m_private->m_light_list_new);
			}
			m_private->m_light_list.insert(m_private->m_light_list.end(), std::make_move_iterator(light.begin()), std::make_move_iterator(light.end()));
		}

		// 更新と寿命の尽きたライトの削除
		m_private->m_light_list.erase(std::remove_if(m_private->m_light_list.begin(), m_private->m_light_list.end(), [](auto& p) { return !p->update(); }), m_private->m_light_list.end());

		{
			// ライトのデータをdeviceにコピー
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

				auto to_copy_barrier = m_private->m_light_info_gpu.getAllocateMemory().makeMemoryBarrierEx();
				to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

				m_private->m_light_info.m_active_light_num = index;
				m_private->m_light_info_gpu.subupdate(m_private->m_light_info);
				m_private->m_light_info_gpu.update(cmd);

				auto to_shader_read_barrier = m_private->m_light_info_gpu.getAllocateMemory().makeMemoryBarrierEx();
				to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
			}

			vk::BufferCopy copy_info;
			copy_info.size = index * sizeof(LightParam);
			copy_info.srcOffset = m_private->m_light_cpu.getBufferInfo().offset + frame_offset * sizeof(LightParam);
			copy_info.dstOffset = m_private->m_light.getBufferInfo().offset;

			auto to_copy_barrier = m_private->m_light.makeMemoryBarrierEx();
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_copy_barrier.setSize(copy_info.size);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			cmd.copyBuffer(m_private->m_light_cpu.getBufferInfo().buffer, m_private->m_light.getBufferInfo().buffer, copy_info);

			auto to_shader_read_barrier = m_private->m_light.makeMemoryBarrierEx();
			to_shader_read_barrier.setSize(copy_info.size);
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}

		{

			{
				std::vector<vk::BufferMemoryBarrier> barrier = {
					m_private->m_lightLL_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
					m_private->m_lightLL.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead)
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, barrier, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_private->m_pipeline[COMPUTE_CULL_LIGHT]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_private->m_pipeline_layout[COMPUTE_CULL_LIGHT], 0, m_private->m_compute_descriptor_set[COMPUTE_CULL_LIGHT], {});
			cmd.dispatch(1, 1, 1);
			{
				std::vector<vk::BufferMemoryBarrier> barrier = {
					m_private->m_lightLL_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite),
					m_private->m_lightLL.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite)
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, barrier, {});
			}
		}

	}

	void executeDebug(vk::CommandBuffer cmd)
	{

	}

};

