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
		Shader_MakeReachableMap_Precompute,
		Shader_MakeReachableMap,
		Shader_MakeReachMap_Precompute,
		Shader_MakeReachMap,
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
		Pipeline_MakeReachableMap_Precompute,
		Pipeline_MakeReachableMap,
		Pipeline_MakeReachMap_Precompute,
		Pipeline_MakeReachMap,
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
				"GI2DPath_MakeReachableMap_Precompute.comp.spv",
				"GI2DPath_MakeReachableMap.comp.spv",
				"GI2DPath_MakeReachMap_Precompute.comp.spv",
				"GI2DPath_MakeReachMap.comp.spv",
				"GI2DSDF_MakeJFA.comp.spv",
				"GI2DSDF_MakeJFA_EX.comp.spv",
				"GI2DSDF_MakeSDF.comp.spv",
				"GI2DSDF_RenderSDF.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
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
			m_pipeline_layout[PipelineLayout_Hierarchy] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
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
			m_pipeline_layout[PipelineLayout_SDF] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
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
			m_pipeline_layout[PipelineLayout_RenderSDF] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Path),
			};
			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(48).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayout_Path] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
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
			shader_info[2].setModule(m_shader[Shader_MakeReachableMap_Precompute].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_MakeReachableMap].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader[Shader_MakeReachMap_Precompute].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			shader_info[5].setModule(m_shader[Shader_MakeReachMap].get());
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
				.setLayout(m_pipeline_layout[PipelineLayout_Path].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Path].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[4])
				.setLayout(m_pipeline_layout[PipelineLayout_Path].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[5])
				.setLayout(m_pipeline_layout[PipelineLayout_Path].get()),
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
			m_pipeline[Pipeline_MakeFragmentMap] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_MakeFragmentMapAndSDF] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
			m_pipeline[Pipeline_MakeReachableMap_Precompute] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
			m_pipeline[Pipeline_MakeReachableMap] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
			m_pipeline[Pipeline_MakeReachMap_Precompute] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[4]).value;
			m_pipeline[Pipeline_MakeReachMap] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[5]).value;
			m_pipeline[Pipeline_MakeJFA] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[6]).value;
			m_pipeline[Pipeline_MakeJFA_EX] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[7]).value;
			m_pipeline[Pipeline_MakeSDF] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[8]).value;
			m_pipeline[Pipeline_RenderSDF] = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[9]).value;
		}

	}
	void executeMakeFragmentMap(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		vk::DescriptorSet desc[] = {
			m_gi2d_context->getDescriptorSet(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Hierarchy].get(), 0, array_length(desc), desc, 0, nullptr);

		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFragmentMap].get());

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->m_desc.Resolution.x, m_gi2d_context->m_desc.Resolution.y, 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	void executeMakeFragmentMapAndSDF(vk::CommandBuffer cmd, const std::shared_ptr<GI2DSDF>& sdf_context)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		vk::DescriptorSet desc[] = {
			m_gi2d_context->getDescriptorSet(),
			sdf_context->getDescriptorSet(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_SDF].get(), 0, array_length(desc), desc, 0, nullptr);
		// make fragment map
		{
			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeFragmentMapAndSDF].get());

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->m_desc.Resolution.x, m_gi2d_context->m_desc.Resolution.y, 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		_executeMakeSDF(cmd, sdf_context);
	}

	void _executeMakeSDF(vk::CommandBuffer cmd, const std::shared_ptr<GI2DSDF>& sdf_context)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		vk::DescriptorSet desc[] = {
			m_gi2d_context->getDescriptorSet(),
			sdf_context->getDescriptorSet(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_SDF].get(), 0, array_length(desc), desc, 0, nullptr);

		// make sdf
		{
			_label.insert("Make JFA");
			int distance = sdf_context->m_gi2d_context->m_desc.Resolution.x >> 1;
#if	1
			// ˆê“x‚É4—v‘fŒvŽZ‚·‚éÅ“K‰»‚ð‚µ‚½
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeJFA_EX].get());
			{
				auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->m_desc.Resolution.x >> 2, sdf_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(64, 1, 1));
//				auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->m_desc.Resolution.x >> 2, sdf_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
				for (; distance >= 4; distance >>= 1)
//				for (; distance < (sdf_context->m_gi2d_context->m_desc.Resolution.x >> 2); distance <<= 1)
				{
					vk::BufferMemoryBarrier to_read[] = { sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite), };
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

					cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_SDF].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, 0 });
					cmd.dispatch(num.x, num.y, num.z);
				}
			}
#endif

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeJFA].get());
			{
				auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->m_desc.Resolution.x, sdf_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(8, 8, 1));
//				auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->m_desc.Resolution.x, sdf_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
				for (; distance > 0; distance >>= 1)
//				for (; distance < sdf_context->m_gi2d_context->m_desc.Resolution.x; distance <<= 1)
				{
					vk::BufferMemoryBarrier to_read[] = { sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite), };
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

					cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_SDF].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, 0 });
					cmd.dispatch(num.x, num.y, num.z);
				}

			}
		}

		_label.insert("Make SDF");
		{
			vk::BufferMemoryBarrier to_read[] = {
				sdf_context->b_jfa.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				sdf_context->b_sdf.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				sdf_context->b_edge.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeSDF].get());
			auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->m_desc.Resolution.x, sdf_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		vk::BufferMemoryBarrier to_read[] = {
			sdf_context->b_sdf.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			sdf_context->b_edge.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
	void executeRenderSDF(vk::CommandBuffer cmd, const std::shared_ptr<GI2DSDF>& sdf_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		vk::DescriptorSet desc[] = {
			m_gi2d_context->getDescriptorSet(),
			sdf_context->getDescriptorSet(),
			render_target->m_descriptor.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_RenderSDF].get(), 0, array_length(desc), desc, 0, nullptr);

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
			auto num = app::calcDipatchGroups(uvec3(sdf_context->m_gi2d_context->m_desc.Resolution.x, sdf_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}

	void executeMakeReachableMap(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPathContext>& path_context)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		vk::DescriptorSet desc[] = {
			path_context->m_gi2d_context->getDescriptorSet(),
			path_context->getDescriptorSet(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Path].get(), 0, array_length(desc), desc, 0, nullptr);

		static int iter = 10;
		iter = iter+1;
		iter %= 100000;
		struct
		{
			i16vec2 target[10];
			i16vec2 target_num;
			i16vec2 reso;
		} constant{ {i16vec2(171, 171), i16vec2(1002, 1002), i16vec2(144, 844), i16vec2(855, 255)}, i16vec2(1, iter / 100), path_context->m_gi2d_context->m_desc.Resolution };
		cmd.pushConstants(m_pipeline_layout[PipelineLayout_Path].get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(constant), &constant);

		vk::BufferMemoryBarrier barrier[] = {
			path_context->b_path_data.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferWrite),
			path_context->b_connect.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_length(barrier), barrier }, {});
		cmd.fillBuffer(path_context->b_path_data.getInfo().buffer, path_context->b_path_data.getInfo().offset, path_context->b_path_data.getInfo().range, -1);
		cmd.fillBuffer(path_context->b_connect.getInfo().buffer, path_context->b_connect.getInfo().offset, path_context->b_connect.getInfo().range, 0);
		
		_label.insert("executeMakeReachableMap_Precompute");
		{
			vk::BufferMemoryBarrier barrier[] = {
				path_context->b_neighbor.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(barrier), barrier }, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeReachableMap_Precompute].get());

			auto num = app::calcDipatchGroups(uvec3(path_context->m_gi2d_context->m_desc.Resolution.x, path_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("executeMakeReachableMap");
		{
			vk::BufferMemoryBarrier barrier[] = {
				path_context->b_neighbor.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				path_context->b_closed.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				path_context->b_path_data.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(barrier), barrier }, {});
			
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeReachableMap].get());

			cmd.dispatch(1, 1, 1);
		}
	}

	void executeMakeReachMap(vk::CommandBuffer cmd, const std::shared_ptr<GI2DPathContext>& path_context)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		vk::DescriptorSet desc[] = {
			path_context->m_gi2d_context->getDescriptorSet(),
			path_context->getDescriptorSet(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Path].get(), 0, array_length(desc), desc, 0, nullptr);

		static int iter = 24600;
//		iter = iter + 1;
		iter %= 40000;
		struct
		{
			i16vec2 target[10];
			i16vec2 target_num;
			i16vec2 reso;
		} constant{ {i16vec2(171, 171), i16vec2(1002, 1002), i16vec2(144, 844), i16vec2(855, 255)}, i16vec2(1, iter / 30), path_context->m_gi2d_context->m_desc.Resolution };
		cmd.pushConstants(m_pipeline_layout[PipelineLayout_Path].get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(constant), &constant);

		vk::BufferMemoryBarrier barrier[] = {
			path_context->b_path_data.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferWrite),
			path_context->b_connect.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_length(barrier), barrier }, {});
		cmd.fillBuffer(path_context->b_path_data.getInfo().buffer, path_context->b_path_data.getInfo().offset, path_context->b_path_data.getInfo().range, -1);
		cmd.fillBuffer(path_context->b_connect.getInfo().buffer, path_context->b_connect.getInfo().offset, path_context->b_connect.getInfo().range, 0);

		_label.insert("executeMakeReachMap_Precompute");
		{
			vk::BufferMemoryBarrier barrier[] = {
				path_context->b_neighbor.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(barrier), barrier }, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeReachMap_Precompute].get());

			auto num = app::calcDipatchGroups(uvec3(path_context->m_gi2d_context->m_desc.Resolution.x, path_context->m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("executeMakeReachMap");
		{
			vk::BufferMemoryBarrier barrier[] = {
				path_context->b_neighbor.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				path_context->b_closed.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				path_context->b_path_data.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(barrier), barrier }, {});

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

