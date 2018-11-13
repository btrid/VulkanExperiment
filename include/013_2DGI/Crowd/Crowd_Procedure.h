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

		Shader_MakeRay,
		Shader_SortRay,
		Shader_MakeSegment,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Crowd,
		PipelineLayout_MakeRay,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_UnitUpdate,
		Pipeline_MakeLinkList,
		Pipeline_MakeDensity,
		Pipeline_DrawDensity,

		Pipeline_MakeRay,
		Pipeline_SortRay,
		Pipeline_MakeSegment,

		Pipeline_Num,
	};
	Crowd_Procedure(const std::shared_ptr<CrowdContext>& context, const std::shared_ptr<gi2d::GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		auto cmd = context->m_context->m_cmd_pool->allocCmdTempolary(0);

		{
			const char* name[] =
			{
				"Crowd_UnitUpdate.comp.spv",
				"Crowd_MakeLinkList.comp.spv",				
				"Crowd_MakeDensityMap.comp.spv",
				"Crowd_DrawDensityMap.comp.spv",

				"Crowd_RayMake.comp.spv",
				"Crowd_RaySort.comp.spv",
				"Crowd_SegmentMake.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader_module[i] = loadShaderUnique(context->m_context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					context->getDescriptorSetLayout(),
					sSystem::Order().getSystemDescriptorLayout(),
					gi2d_context->getDescriptorSetLayout(),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_Crowd] = context->m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

			}

			{
				vk::DescriptorSetLayout layouts[] =
				{
					context->getDescriptorSetLayout(),
				};
				vk::PushConstantRange constants[] =
				{
					vk::PushConstantRange().setOffset(0).setSize(16).setStageFlags(vk::ShaderStageFlagBits::eCompute),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(std::size(constants));
				pipeline_layout_info.setPPushConstantRanges(constants);
				m_pipeline_layout[PipelineLayout_MakeRay] = context->m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

			}
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 7> shader_info;
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
			shader_info[4].setModule(m_shader_module[Shader_MakeRay].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			shader_info[5].setModule(m_shader_module[Shader_SortRay].get());
			shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[5].setPName("main");
			shader_info[6].setModule(m_shader_module[Shader_MakeSegment].get());
			shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[6].setPName("main");
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
				.setLayout(m_pipeline_layout[PipelineLayout_MakeRay].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[5])
				.setLayout(m_pipeline_layout[PipelineLayout_MakeRay].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[6])
				.setLayout(m_pipeline_layout[PipelineLayout_Crowd].get()),
			};
			auto compute_pipeline = context->m_context->m_device->createComputePipelinesUnique(context->m_context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_UnitUpdate] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_MakeLinkList] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_MakeDensity] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_DrawDensity] = std::move(compute_pipeline[3]);
			m_pipeline[Pipeline_MakeRay] = std::move(compute_pipeline[4]);
			m_pipeline[Pipeline_SortRay] = std::move(compute_pipeline[5]);
			m_pipeline[Pipeline_MakeSegment] = std::move(compute_pipeline[6]);
		}
	}

	void executeUpdateUnit(vk::CommandBuffer cmd)
	{
		// カウンターのclear
		{
			cmd.updateBuffer<uvec4>(m_context->b_unit_counter.getInfo().buffer, m_context->b_unit_counter.getInfo().offset, uvec4(0));
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_unit_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
				m_context->b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer| vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}

		// 更新
		{
			vk::DescriptorSet descriptors[] =
			{
				m_context->getDescriptorSet(),
				sSystem::Order().getSystemDescriptorSet(),
				m_gi2d_context->getDescriptorSet(),
			};

			uint32_t offset[array_length(descriptors)] = {};
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_UnitUpdate].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);
			cmd.dispatch(1, 1, 1);
		}

	}
	void executeMakeLinkList(vk::CommandBuffer cmd)
	{
		// カウンターのclear
		{
			cmd.fillBuffer(m_context->b_unit_link_head.getInfo().buffer, m_context->b_unit_link_head.getInfo().offset, m_context->b_unit_link_head.getInfo().range, -1);
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_unit_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}

		// 更新
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeLinkList].get());
			vk::DescriptorSet descriptors[] =
			{
				m_context->getDescriptorSet(),
				sSystem::Order().getSystemDescriptorSet(),
				m_gi2d_context->getDescriptorSet(),
			};
			uint32_t offset[array_length(descriptors)] = {};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);
			cmd.dispatch(1, 1, 1);
		}

	}
	void executeMakeRay(vk::CommandBuffer cmd)
	{

		{
			// データクリア
			std::vector<ivec4> data = std::vector<ivec4>(m_context->m_crowd_info.frame_max, ivec4(0, 1, 1, 0));
			cmd.updateBuffer(m_context->b_ray_counter.getInfo().buffer, m_context->b_ray_counter.getInfo().offset, vector_sizeof(data), data.data());
		}

		vk::DescriptorSet descriptors[] =
		{
			m_context->getDescriptorSet(),
		};
		uint32_t offset[array_length(descriptors)] = {};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Crowd].get(), 0, array_length(descriptors), descriptors, array_length(offset), offset);

		{
			// レイの生成
			vk::BufferMemoryBarrier to_read[] = {
				m_context->b_ray_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			auto num = app::calcDipatchGroups(uvec3(8192, 64, m_context->m_crowd_info.frame_max), uvec3(64, 16, 1));

			cmd.pushConstants<std::tuple<ivec2, float, uint>>(m_pipeline_layout[PipelineLayout_MakeRay].get(), vk::ShaderStageFlagBits::eCompute, 0, { std::make_tuple(ivec2(8192), 4.f, 64) });
			cmd.dispatch(num.x, num.y, num.z);
		}

		{
			// レイのソート
			{
				vk::BufferMemoryBarrier to_read[] = {
					m_context->b_ray_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					m_context->b_ray.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

			}

			for (int step = 2; step <= m_context->m_crowd_info.ray_num_max; step <<= 1) {
				for (int offset = step >> 1; offset > 0; offset = offset >> 1) {
					cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayout_MakeRay].get(), vk::ShaderStageFlagBits::eCompute, 0, ivec2(step, offset));
					auto num = app::calcDipatchGroups(uvec3(m_context->m_crowd_info.ray_num_max, 1, 1), uvec3(1024, 1, 1));
					cmd.dispatch(num.x, num.y, num.z);

					vk::BufferMemoryBarrier to_read[] = {
						m_context->b_ray.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
						0, nullptr, array_length(to_read), to_read, 0, nullptr);
				}
			}
		}
	}

	void executePathFinding(vk::CommandBuffer cmd)
	{

	}

	std::shared_ptr<CrowdContext> m_context;
	std::shared_ptr<gi2d::GI2DContext> m_gi2d_context;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader_module;


};