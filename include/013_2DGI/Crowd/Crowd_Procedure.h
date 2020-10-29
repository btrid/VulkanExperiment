#pragma once
#include <013_2DGI/Crowd/CrowdContext.h>
#include <applib/sSystem.h>
#include <013_2DGI/GI2D/GI2DContext.h>


struct Crowd_Procedure
{
	enum Shader
	{
		Shader_UnitUpdate,
		Shader_MakeLinkList,
		Shader_MakeDensity,
		Shader_DrawDensity,

		Shader_Render,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Crowd,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_UnitUpdate,
		Pipeline_MakeLinkList,
		Pipeline_MakeDensity,
		Pipeline_DrawDensity,
		Pipeline_Render,

		Pipeline_Num,
	};
	Crowd_Procedure(const std::shared_ptr<CrowdContext>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<GI2DSDF> sdf_context, const std::shared_ptr<GI2DPathContext>& path_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
		m_sdf_context = sdf_context;
		m_path_context = path_context;


		auto cmd = context->m_context->m_cmd_pool->allocCmdTempolary(0);

		{
			const char* name[] =
			{
				"Crowd_UnitUpdate.comp.spv",
				"Crowd_MakeLinkList.comp.spv",
				"Crowd_MakeDensityMap.comp.spv",
				"Crowd_DrawDensityMap.comp.spv",
				"Crowd_Render.comp.spv",

			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader_module[i] = loadShaderUnique(context->m_context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{

			{
				vk::DescriptorSetLayout layouts[] = {
					context->getDescriptorSetLayout(),
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Path),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(std::size(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_Crowd] = context->m_context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 5> shader_info;
			shader_info[0].setModule(m_shader_module[Shader_UnitUpdate].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader_module[Shader_MakeLinkList].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader_module[Shader_MakeDensity].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader_module[Shader_DrawDensity].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader_module[Shader_Render].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[4])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
			};
			auto compute_pipeline = context->m_context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_UnitUpdate] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_MakeLinkList] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_MakeDensity] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_DrawDensity] = std::move(compute_pipeline[3]);
			m_pipeline[Pipeline_Render] = std::move(compute_pipeline[4]);
		}
	}

	void executeUpdateUnit(vk::CommandBuffer cmd)
	{
		_executeMakeLinkList(cmd);

		// 更新
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_UnitUpdate].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, { m_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 1, { m_gi2d_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 2, { m_sdf_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 3, { m_path_context->getDescriptorSet() }, {});
			auto num = app::calcDipatchGroups(uvec3(m_context->m_crowd_info.unit_data_max, 1, 1), uvec3(64, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	void _executeMakeLinkList(vk::CommandBuffer cmd)
	{
		// カウンターのclear
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_context->b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, 0, {}, array_length(to_write), to_write, 0, {});

			cmd.fillBuffer(m_context->b_unit_link_head.getInfo().buffer, m_context->b_unit_link_head.getInfo().offset, m_context->b_unit_link_head.getInfo().range, -1);
		}

		// 更新
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
				m_context->b_unit_link_next.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_context->b_unit_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader|vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeLinkList].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, { m_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 1, { m_gi2d_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 2, { m_sdf_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 3, { m_path_context->getDescriptorSet() }, {});
			auto num = app::calcDipatchGroups(uvec3(m_context->m_crowd_info.unit_data_max, 1, 1), uvec3(64, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}


		// barrier
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_context->b_unit_link_next.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}
	}
	void executeRendering(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		{
			vk::BufferMemoryBarrier barrier[] = {
				m_context->b_unit_pos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(render_target->m_image);
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eGeneral);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, { image_barrier });

		}
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Render].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, { m_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 1, { m_gi2d_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 2, { m_sdf_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 3, { m_path_context->getDescriptorSet() }, {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 4, { render_target->m_descriptor.get() }, {});

			auto num = app::calcDipatchGroups(uvec3(m_context->m_crowd_info.unit_data_max, 1, 1), uvec3(64, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// 更新
		{
// 			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeLinkList].get());
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, { m_context->getDescriptorSet() }, {});
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 1, { sSystem::Order().getSystemDescriptorSet() }, { 0 });
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 2, { m_gi2d_context->getDescriptorSet() }, {});
// 			cmd.dispatch(1, 1, 1);
		}

	}

	std::shared_ptr<CrowdContext> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<GI2DSDF> m_sdf_context;
	std::shared_ptr<GI2DPathContext> m_path_context;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader_module;


};