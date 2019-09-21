#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/sCameraManager.h>

#include <006_voxel_based_GI/ModelVoxelize.h>

struct VoxelRender
{
	enum SHADER
	{
		SHADER_DRAW_VOXEL_VS,
		SHADER_DRAW_VOXEL_FS,
		SHADER_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_DRAW_VOXEL,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_VOXEL,
		PIPELINE_NUM,
	};

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<VoxelContext_Old> m_vx_context;
	std::shared_ptr<RenderTarget> m_render_target;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	VoxelRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<VoxelContext_Old>& vx_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = context;
		m_vx_context = vx_context;
		m_render_target = render_target;


		auto& gpu = context->m_gpu;
		auto& device = gpu.getDevice();

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		// レンダーパス
		{
			// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);
			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(render_target->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);

			{
				vk::ImageView view[] = {
					render_target->m_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(render_target->m_info.extent.width);
				framebuffer_info.setHeight(render_target->m_info.extent.height);
				framebuffer_info.setLayers(1);
				m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}
		}
		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "DrawVoxel.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "DrawVoxel.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceShaderPath() + "../006_voxel_based_GI/binary/";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_list[i] = std::move(loadShaderUnique(device.get(), path + shader_info[i].name));
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_vx_context->getDescriptorSetLayout(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};
				vk::PushConstantRange constants[] = {
					vk::PushConstantRange()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex)
					.setSize(4)
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
				pipeline_layout_info.setPPushConstantRanges(constants);
				m_pipeline_layout[PIPELINE_DRAW_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);
			}
		}

		// pipeline
		{
			{
				// assembly
				vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
					.setPrimitiveRestartEnable(VK_FALSE)
					.setTopology(vk::PrimitiveTopology::eTriangleStrip);

				// viewport
				vk::Viewport viewports[] = {
					vk::Viewport(0.f, 0.f, (float)context->m_window->getClientSize().x, (float)context->m_window->getClientSize().y),
				};
				vk::Rect2D scissors[] = {
					vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()),
				};
				vk::PipelineViewportStateCreateInfo viewport_info;
				viewport_info.setViewportCount(array_length(viewports));
				viewport_info.setPViewports(viewports);
				viewport_info.setScissorCount(array_length(scissors));
				viewport_info.setPScissors(scissors);

				// ラスタライズ
				vk::PipelineRasterizationStateCreateInfo rasterization_info;
				rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
				rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
				rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
				rasterization_info.setLineWidth(1.f);

				// サンプリング
				vk::PipelineMultisampleStateCreateInfo sample_info;
				sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

				// デプスステンシル
				vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
				depth_stencil_info.setDepthTestEnable(VK_TRUE);
				depth_stencil_info.setDepthWriteEnable(VK_TRUE);
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
				depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
				depth_stencil_info.setStencilTestEnable(VK_FALSE);

				// ブレンド
				vk::PipelineColorBlendAttachmentState blend_states[] = {
					vk::PipelineColorBlendAttachmentState()
					.setBlendEnable(VK_FALSE)
					.setColorWriteMask(vk::ColorComponentFlagBits::eR
						| vk::ColorComponentFlagBits::eG
						| vk::ColorComponentFlagBits::eB
						| vk::ColorComponentFlagBits::eA)
				};
				vk::PipelineColorBlendStateCreateInfo blend_info;
				blend_info.setAttachmentCount(array_length(blend_states));
				blend_info.setPAttachments(blend_states);

				vk::PipelineVertexInputStateCreateInfo vertex_input_info;

				vk::PipelineShaderStageCreateInfo stage_infos[] =
				{
					vk::PipelineShaderStageCreateInfo()
					.setModule(m_shader_list[SHADER_DRAW_VOXEL_VS].get())
					.setStage(vk::ShaderStageFlagBits::eVertex)
					.setPName("main"),
					vk::PipelineShaderStageCreateInfo()
					.setModule(m_shader_list[SHADER_DRAW_VOXEL_FS].get())
					.setStage(vk::ShaderStageFlagBits::eFragment)
					.setPName("main"),
				};
				std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
				{
					vk::GraphicsPipelineCreateInfo()
					.setStageCount(array_length(stage_infos))
					.setPStages(stage_infos)
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&assembly_info)
					.setPViewportState(&viewport_info)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL].get())
					.setRenderPass(m_render_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info),
				};
				m_pipeline[PIPELINE_DRAW_VOXEL] = std::move(device->createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info)[0]);
			}

		}

	}

	void execute(vk::CommandBuffer cmd)
	{
		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(5);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);
		{
			// draw voxel
			{
				vk::ImageMemoryBarrier to_read_barrier;
				to_read_barrier.image = m_vx_context->m_voxel_hierarchy_image.get();
				to_read_barrier.oldLayout = vk::ImageLayout::eGeneral;
				to_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_read_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
			}
			vk::RenderPassBeginInfo begin_render_info;
			begin_render_info.setFramebuffer(m_framebuffer.get());
			begin_render_info.setRenderPass(m_render_pass.get());
			begin_render_info.setRenderArea(vk::Rect2D({}, m_context->m_window->getClientSize<vk::Extent2D>()));

			cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_VOXEL].get());
			std::vector<vk::DescriptorSet> descriptor_sets = {
				m_vx_context->getDescriptorSet(),
				sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL].get(), 0, descriptor_sets, {});
			uint32_t miplevel = 0;
			cmd.pushConstants<uint32_t>(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL].get(), vk::ShaderStageFlagBits::eVertex, 0, miplevel);

			cmd.draw(14, m_vx_context->m_voxelize_info_cpu.u_cell_num.x *m_vx_context->m_voxelize_info_cpu.u_cell_num.y*m_vx_context->m_voxelize_info_cpu.u_cell_num.z / (1 << (3 * miplevel)), 0, 0);
			cmd.endRenderPass();
		}

	}


};