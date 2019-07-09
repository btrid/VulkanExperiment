#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct GI2DRadiosity3
{
	enum Shader
	{
		Shader_Precompute,
		Shader_Compute,

		Shader_Rendering,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Radiosity,
		PipelineLayout_Rendering,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Precompute,
		Pipeline_Compute,

		Pipeline_Rendering,

		Pipeline_Num,
	};
	GI2DRadiosity3(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
		m_render_target = render_target;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			b_closed = context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->FragmentBufferSize / 32,{} });
			b_neighbor = context->m_storage_memory.allocateMemory<uint8_t>({ gi2d_context->FragmentBufferSize,{} });
			b_radiance = context->m_storage_memory.allocateMemory<f16vec4>({ gi2d_context->FragmentBufferSize,{} });
		}

		{

			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
					b_closed.getInfo(),
					b_neighbor.getInfo(),
					b_radiance.getInfo(),
				};

				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}

		{
			const char* name[] =
			{
				"Radiosity3_Precompute.comp.spv",
				"Radiosity3_Compute.comp.spv",

				"Radiosity3_Rendering.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(16).setStageFlags(vk::ShaderStageFlagBits::eCompute),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_pipeline_layout[PipelineLayout_Radiosity] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

			}

			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					m_descriptor_set_layout.get(),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(16).setStageFlags(vk::ShaderStageFlagBits::eCompute),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_pipeline_layout[PipelineLayout_Rendering] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

			}

		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 10> shader_info;
			shader_info[0].setModule(m_shader[Shader_Precompute].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_Compute].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_Rendering].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Rendering].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_Precompute] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_Compute] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_Rendering] = std::move(compute_pipeline[2]);
		}


	}
	void executeRadiosity(const vk::CommandBuffer& cmd)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		{
			vk::BufferMemoryBarrier to_write[] = {
				b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.fillBuffer(b_radiance.getInfo().buffer, b_radiance.getInfo().offset, b_radiance.getInfo().range, 0);

		}

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});

		struct
		{
			i16vec2 target;
			i16vec2 reso;
			f16vec4 light;
		} constant{ i16vec2(444, 666), m_gi2d_context->RenderSize, glm::packHalf4x16(vec4(1000.f))};
		cmd.pushConstants(m_pipeline_layout[PipelineLayout_Radiosity].get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(constant), &constant);

		_label.insert("execute_Precompute");
		{
			vk::BufferMemoryBarrier barrier[] = {
				b_neighbor.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(barrier), barrier }, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Precompute].get());

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("execute_Compute");
		{
			vk::BufferMemoryBarrier barrier[] = {
				b_neighbor.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(barrier), barrier }, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Compute].get());

			cmd.dispatch(1, 1, 1);
		}
	}

	void executeRendering(const vk::CommandBuffer& cmd)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__, DebugLabel::k_color_debug);

		vk::BufferMemoryBarrier barrier[] = {
			b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		vk::ImageMemoryBarrier image_barrier;
		image_barrier.setImage(m_render_target->m_image);
		image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
		image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
		image_barrier.setNewLayout(vk::ImageLayout::eGeneral);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, { image_barrier });


		vk::DescriptorSet descriptorsets[] = {
			m_gi2d_context->getDescriptorSet(),
			m_descriptor_set.get(),
			m_render_target->m_descriptor.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rendering].get(), 0, array_length(descriptorsets), descriptorsets, 0, nullptr);

		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Rendering].get());

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderSize.x, m_gi2d_context->RenderSize.y, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<RenderTarget> m_render_target;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<uint32_t> b_closed;
	btr::BufferMemoryEx<uint8_t> b_neighbor;
	btr::BufferMemoryEx<f16vec4> b_radiance;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

};

