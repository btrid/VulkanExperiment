#pragma once
#include <array>
#include <memory>
#include <mutex>
#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>

struct cModelInstancingRenderer;

struct LightData {
	glm::vec4 m_position;
	glm::vec4 m_emission;
};

struct Light
{
	virtual bool update()
	{
		return true;
	}

	virtual LightData getParam()const
	{
		return LightData();
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
		SHADER_COMPUTE_CULL_LIGHT,
		SHADER_NUM,
	};

	enum DescriptorSet
	{
		DESCRIPTOR_SET_LIGHT,
		DESCRIPTOR_SET_NUM,
	};
	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_LIGHT,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_CULL_LIGHT,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_CULL_LIGHT,
		PIPELINE_NUM,
	};

	vk::DescriptorBufferInfo getLightInfoBufferInfo()const { return m_private->m_light_info_gpu.getBufferInfo(); }
	vk::DescriptorBufferInfo getLightBufferInfo()const { return m_private->m_light.getBufferInfo(); }
	vk::DescriptorBufferInfo getLightLLHeadBufferInfo()const { return m_private->m_lightLL_head.getBufferInfo(); }
	vk::DescriptorBufferInfo getLightLLBufferInfo()const { return m_private->m_lightLL.getBufferInfo(); }

	struct Private
	{
		LightInfo m_light_info;
		btr::UpdateBuffer<LightInfo> m_light_info_gpu;
		btr::UpdateBuffer<FrustomPoint> m_frustom_point;
		btr::UpdateBuffer<LightData> m_light;
		btr::BufferMemory m_lightLL_head;
		btr::BufferMemory m_lightLL;
		btr::BufferMemory m_light_counter;

		uint32_t m_light_num;

		std::vector<std::unique_ptr<Light>> m_light_list;
		std::vector<std::unique_ptr<Light>> m_light_list_new;
		std::mutex m_light_new_mutex;

		std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
		std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
		std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
		std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

		std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
		std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
		std::array<vk::PipelineShaderStageCreateInfo, 2> m_shader_info_debug;

		void setup(const std::shared_ptr<btr::Context>& context);
	};
	std::unique_ptr<Private> m_private;

	void setup(const std::shared_ptr<btr::Context>& context)
	{
		m_private = std::make_unique<Private>();
		m_private->setup(context);
	}

	void add(std::unique_ptr<Light>&& light)
	{
		m_private->m_light_list_new.push_back(std::move(light));
	}

	vk::CommandBuffer execute(const std::shared_ptr<btr::Context>& context)
	{

		// lightの更新
		{
			// 新しいライトの追加
			std::vector<std::unique_ptr<Light>> light;
			{
				std::lock_guard<std::mutex> lock(m_private->m_light_new_mutex);
				light = std::move(m_private->m_light_list_new);
			}
			m_private->m_light_list.insert(m_private->m_light_list.end(), std::make_move_iterator(light.begin()), std::make_move_iterator(light.end()));
			// 更新と寿命の尽きたライトの削除
			m_private->m_light_list.erase(std::remove_if(m_private->m_light_list.begin(), m_private->m_light_list.end(), [](auto& p) { return !p->update(); }), m_private->m_light_list.end());
		}


		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		// cpuからの更新のバリア
		{
			auto to_copy_barrier = m_private->m_light_counter.makeMemoryBarrierEx();
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
		}
		{
			uint32_t zero = 0;
			cmd.updateBuffer<uint32_t>(m_private->m_light_counter.getBufferInfo().buffer, m_private->m_light_counter.getBufferInfo().offset, { zero });
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

			auto to_copy_barrier = m_private->m_frustom_point.getBufferMemory().makeMemoryBarrierEx();
			to_copy_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			to_copy_barrier.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { to_copy_barrier }, {});

			m_private->m_frustom_point.subupdate(&frustom_point, 1, 0, context->getGPUFrame());
			auto copy_info = m_private->m_frustom_point.update(context->getGPUFrame());
			cmd.copyBuffer(m_private->m_frustom_point.getStagingBufferInfo().buffer, m_private->m_frustom_point.getBufferInfo().buffer, copy_info);

			auto to_shader_read_barrier = m_private->m_frustom_point.getBufferMemory().makeMemoryBarrierEx();
			to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
		}

		{
			// ライトのデータをdeviceにコピー
			uint32_t light_num = 0;
			{
				auto* p = m_private->m_light.mapSubBuffer(context->getGPUFrame());
				for (auto& it : m_private->m_light_list)
				{
					assert(light_num < m_private->m_light_num);
					p[light_num] = it->getParam();
					light_num++;
				}

				m_private->m_light.flushSubBuffer(light_num, 0, context->getGPUFrame());
				auto copy_info = m_private->m_light.update(context->getGPUFrame());
				cmd.copyBuffer(m_private->m_light.getStagingBufferInfo().buffer, m_private->m_light.getBufferInfo().buffer, copy_info);
			}
			{

				m_private->m_light_info.m_active_light_num = light_num;
				m_private->m_light_info_gpu.subupdate(&m_private->m_light_info, 1, 0, context->getGPUFrame());
				auto copy_info = m_private->m_light_info_gpu.update(context->getGPUFrame());
				cmd.copyBuffer(m_private->m_light_info_gpu.getStagingBufferInfo().buffer, m_private->m_light_info_gpu.getBufferInfo().buffer, copy_info);

				auto to_shader_read_barrier = m_private->m_light_info_gpu.getBufferMemory().makeMemoryBarrierEx();
				to_shader_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_shader_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});
			}
		}

		{

			{
				std::vector<vk::BufferMemoryBarrier> barrier = {
					m_private->m_light_counter.makeMemoryBarrierEx()
					.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite)
					.setDstAccessMask(vk::AccessFlagBits::eShaderWrite|vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, barrier, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_private->m_pipeline[PIPELINE_CULL_LIGHT].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_private->m_pipeline_layout[PIPELINE_CULL_LIGHT].get(), 0, m_private->m_descriptor_set[DESCRIPTOR_SET_LIGHT].get(), {});
			cmd.dispatch(1, 1, 1);
			{
				std::vector<vk::BufferMemoryBarrier> barrier = {
					m_private->m_lightLL_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite),
					m_private->m_lightLL.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite)
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, barrier, {});
			}
		}

		cmd.end();
		return cmd;
	}

	void executeDebug(vk::CommandBuffer cmd)
	{

	}

};

