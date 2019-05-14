#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct GI2DMakeHierarchy
{
	enum Shader
	{
		Shader_MakeFragmentMap,
		Shader_MakeFragmentMapAndSDF,
		Shader_MakeFragmentMapHierarchy,
		Shader_MakeReachMap_Precompute,
		Shader_MakeReachMap,
		Shader_MakeLight,
		Shader_MakeJFA,
		Shader_MakeJFA_EX,
		Shader_MakeSDF,
		Shader_RenderSDF,
		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Hierarchy,
		PipelineLayout_SDF,
		PipelineLayout_RenderSDF,
		PipelineLayout_Path,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MakeFragmentMap,
		Pipeline_MakeFragmentMapAndSDF,
		Pipeline_MakeFragmentMapHierarchy,
		Pipeline_MakeReachMap_Precompute,
		Pipeline_MakeReachMap,
		Pipeline_MakeLight,
		Pipeline_MakeJFA,
		Pipeline_MakeJFA_EX,
		Pipeline_MakeSDF,
		Pipeline_RenderSDF,
		Pipeline_Num,
	};

	GI2DMakeHierarchy(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		{
			const char* name[] =
			{
				"GI2D_MakeFragmentMap.comp.spv",
				"GI2D_MakeFragmentMapAndSDF.comp.spv",
				"GI2D_MakeFragmentMapHierarchy.comp.spv",
				"GI2DPath_MakeReachMap_Precompute.comp.spv",
				"GI2DPath_MakeReachMap.comp.spv",
				"GI2D_MakeLight.comp.spv",
				"GI2DSDF_MakeJFA.comp.spv",
				"GI2DSDF_MakeJFA_EX.comp.spv",
				"GI2DSDF_MakeSDF.comp.spv",
				"GI2DSDF_RenderSDF.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayout_Hierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(8).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayout_SDF] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
				RenderTarget::s_descriptor_set_layout.get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_RenderSDF] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Path),
			};
			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(16).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayout_Path] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, Pipeline_Num> shader_info;
			shader_info[0].setModule(m_shader[Shader_MakeFragmentMap].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_MakeFragmentMapAndSDF].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_MakeFragmentMapHierarchy].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_MakeReachMap_Precompute].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader[Shader_MakeReachMap].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			shader_info[5].setModule(m_shader[Shader_MakeLight].get());
			shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[5].setPName("main");
			shader_info[6].setModule(m_shader[Shader_MakeJFA].get());
			shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[6].setPName("main");
			shader_info[7].setModule(m_shader[Shader_MakeJFA_EX].get());
			shader_info[7].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[7].setPName("main");
			shader_info[8].setModule(m_shader[Shader_MakeSDF].get());
			shader_info[8].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[8].setPName("main");
			shader_info[9].setModule(m_shader[Shader_RenderSDF].get());
			shader_info[9].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[9].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Hierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_SDF].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Hierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Path].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[4])
				.setLayout(m_pipeline_layout[PipelineLayout_Path].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[5])
				.setLayout(m_pipeline_layout[PipelineLayout_Hierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[6])
				.setLayout(m_pipeline_layout[PipelineLayout_SDF].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[7])
				.setLayout(m_pipeline_layout[PipelineLayout_SDF].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[8])
				.setLayout(m_pipeline_layout[PipelineLayout_SDF].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[9])
				.setLayout(m_pipeline_layout[PipelineLayout_RenderSDF].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_MakeFragmentMap] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_MakeFragmentMapAndSDF] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_MakeFragmentMapHierarchy] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_MakeReachMap_Precompute] = std::move(compute_pipeline[3]);
			m_pipeline[Pipeline_MakeReachMap] = std::move(compute_pipeline[4]);
			m_pipeline[Pipeline_MakeLight] = std::move(compute_pipeline[5]);
			m_pipeline[Pipeline_MakeJFA] = std::move(compute_pipeline[6]);
			m_pipeline[Pipeline_MakeJFA_EX] = std::move(compute_pipeline[7]);
			m_pipeline[Pipeline_MakeSDF] = std::move(compute_pipeline[8]);
			m_pipeline[Pipeline_RenderSDF] = std::move(compute_pipeline[9]);
		}

	}
	void executeMakeFragmentMap(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Hierarchy].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_gi2d_context->b_light.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFragmentMap].get());

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	void executeMakeFragmentMapAndSDF(vk::CommandBuffer cmd, const std::shared_ptr<GI2DSDF>& sdf_context)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_SDF].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_SDF].get(), 1, sdf_context->getDescriptorSet(), {});
		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				m_gi2d_context->b_light.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFragmentMapAndSDF].get());

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}

	void executeHierarchy(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFragmentMapHierarchy].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Hierarchy].get(), 0, m_gi2d_context->getDescriptorSet(), {});

		for (uint32_t i = 1; i < m_gi2d_context->m_gi2d_info.m_hierarchy_num; i++)
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_diffuse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.pushConstants<int32_t>(m_pipeline_layout[PipelineLayout_Hierarchy].get(), vk::ShaderStageFlagBits::eCompute, 0, i);
			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth >> (i*3), m_gi2d_context->RenderHeight >> (i * 3), 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}

	void executeMakeSDF(vk::CommandBuffer cmd, const std::shared_ptr<GI2DSDF>& sdf_context)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_SDF].get(), 0, sdf_context->m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_SDF].get(), 1, sdf_context->getDescriptorSet(), {});

		// make sdf
		{
			_label.insert("Make JFA");
#if	0
			// ˆê“x‚É4—v‘fŒvŽZ‚·‚éÅ“K‰»‚ð‚µ‚½
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeJFA_EX].get());
			{
				auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->RenderWidth>>2, sdf_context->m_gi2d_context->RenderHeight, 1), uvec3(64, 1, 1));
				for (int distance = sdf_context->m_gi2d_context->RenderWidth >> 1; distance >= 4; distance >>= 1)
				{
					for (uint i = 0; i < 2; i++)
					{
						vk::BufferMemoryBarrier to_read[] = { sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite), };
						to_read[0].size /= 2;
						to_read[0].offset += to_read[0].size * i;
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

						cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_SDF].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, i*sdf_context->m_gi2d_context->FragmentBufferSize / 4 });
						cmd.dispatch(num.x, num.y, num.z);
					}
				}
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeJFA].get());
			{
				auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->RenderWidth, sdf_context->m_gi2d_context->RenderHeight, 1), uvec3(64, 1, 1));
				for (int distance = 2; distance != 0; distance >>= 1)
				{
					for (uint i = 0; i < 2; i++)
					{
						vk::BufferMemoryBarrier to_read[] = { sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite), };
						to_read[0].size /= 2;
						to_read[0].offset += to_read[0].size * i;
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

						cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_SDF].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, i*sdf_context->m_gi2d_context->FragmentBufferSize });
						cmd.dispatch(num.x, num.y, num.z);
					}
				}

			}

#else
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeJFA].get());
			{
				auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->RenderWidth, sdf_context->m_gi2d_context->RenderHeight, 1), uvec3(64, 1, 1));
				for (int distance = sdf_context->m_gi2d_context->RenderWidth >> 1; distance != 0; distance >>= 1)
				{
					vk::BufferMemoryBarrier to_read[] = { sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite), };
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
					for (uint i = 0; i < 2; i++)
					{
//						vk::BufferMemoryBarrier to_read[] = { sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite), };
//						to_read[0].size /= 2;
//						to_read[0].offset += to_read[0].size * i;
						cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_SDF].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, i*sdf_context->m_gi2d_context->FragmentBufferSize });
						cmd.dispatch(num.x, num.y, num.z);
					}
				}
			}
#endif
		}

		_label.insert("Make SDF");
		{
			vk::BufferMemoryBarrier to_read[] = {
				sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				sdf_context->b_sdf.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeSDF].get());
			auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->RenderWidth, sdf_context->m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		vk::BufferMemoryBarrier to_read[] = {
			sdf_context->b_sdf.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
	void executeRenderSDF(vk::CommandBuffer cmd, const std::shared_ptr<GI2DSDF>& sdf_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_RenderSDF].get(), 0, sdf_context->m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_RenderSDF].get(), 1, sdf_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_RenderSDF].get(), 2, render_target->m_descriptor.get(), {});

		{
			vk::BufferMemoryBarrier to_read[] = {
				sdf_context->b_sdf.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};

			vk::ImageMemoryBarrier barrier;
			barrier.setImage(render_target->m_image);
			barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
			barrier.setNewLayout(vk::ImageLayout::eGeneral);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(to_read), to_read }, { barrier });

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RenderSDF].get());
			auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->RenderWidth, sdf_context->m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}

	void executeMakeReachMap(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPathContext>& path_context)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		vk::BufferMemoryBarrier barrier[] = {
			path_context->b_access.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Path].get(), 0, path_context->m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Path].get(), 1, path_context->getDescriptorSet(), {});

		struct
		{
			ivec2 reso;
			i16vec2 target1;
			i16vec2 target2;
		} constant{ path_context->m_gi2d_context->RenderSize, i16vec2(11, 11), i16vec2(0) };
		cmd.pushConstants(m_pipeline_layout[PipelineLayout_Path].get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(constant), &constant);

		_label.insert("executeMakeReachMap_Precompute");
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeReachMap_Precompute].get());

			auto num = app::calcDipatchGroups(uvec3(path_context->m_gi2d_context->RenderWidth, path_context->m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("executeMakeReachMap");
		{
 			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeReachMap].get());

			cmd.dispatch(1, 1, 1);
		}
	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

};

