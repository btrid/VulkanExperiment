#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>
#include <999_game/MazeGenerator.h>
#include <applib/Geometry.h>
#include <applib/sCameraManager.h>
#include <999_game/sLightSystem.h>
#include <999_game/sScene.h>

#include <btrlib/VoxelPipeline.h>

struct sMap : public Singleton<sMap>
{

	enum
	{
		SHADER_VERTEX_FLOOR,
		SHADER_GEOMETRY_FLOOR,
		SHADER_FRAGMENT_FLOOR,
		SHADER_VERTEX_FLOOR_EX,
		SHADER_FRAGMENT_FLOOR_EX,
		SHADER_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_DRAW_FLOOR,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_FLOOR,
		PIPELINE_DRAW_FLOOR_EX,
		PIPELINE_NUM,
	};

	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;
	std::vector<vk::UniqueCommandBuffer> m_cmd;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	VoxelPipeline m_voxelize_pipeline;

	void setup(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			// レンダーパス
			{
				// sub pass
				std::vector<vk::AttachmentReference> color_ref =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(1);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount((uint32_t)color_ref.size());
				subpass.setPColorAttachments(color_ref.data());
				subpass.setPDepthStencilAttachment(&depth_ref);

				std::vector<vk::AttachmentDescription> attach_description = {
					// color1
					vk::AttachmentDescription()
					.setFormat(context->m_window->getSwapchain().m_surface_format.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					vk::AttachmentDescription()
					.setFormat(vk::Format::eD32Sfloat)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
					.setAttachmentCount(attach_description.size())
					.setPAttachments(attach_description.data())
					.setSubpassCount(1)
					.setPSubpasses(&subpass);

				m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
			}

			m_framebuffer.resize(context->m_window->getSwapchain().getBackbufferNum());
			{
				std::array<vk::ImageView, 2> view;

				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass.get());
				framebuffer_info.setAttachmentCount((uint32_t)view.size());
				framebuffer_info.setPAttachments(view.data());
				framebuffer_info.setWidth(context->m_window->getClientSize().x);
				framebuffer_info.setHeight(context->m_window->getClientSize().y);
				framebuffer_info.setLayers(1);

				for (size_t i = 0; i < m_framebuffer.size(); i++) {
					view[0] = context->m_window->getSwapchain().m_backbuffer[i].m_view;
					view[1] = context->m_window->getSwapchain().m_depth.m_view;
					m_framebuffer[i] = context->m_device->createFramebufferUnique(framebuffer_info);
				}
			}

		}

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}
			shader_info[] =
			{
				{ "FloorRender.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "FloorRender.geom.spv",vk::ShaderStageFlagBits::eGeometry },
				{ "FloorRender.frag.spv",vk::ShaderStageFlagBits::eFragment },
				{ "FloorRenderEx.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "FloorRenderEx.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
				m_shader_info[i].setModule(m_shader_module[i].get());
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}


		{
			vk::DescriptorSetLayout layouts[] = {
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_SCENE),
				sLightSystem::Order().getDescriptorSetLayout(sLightSystem::DESCRIPTOR_SET_LAYOUT_LIGHT),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		vk::Extent3D size;
		size.setWidth(640);
		size.setHeight(480);
		size.setDepth(1);
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList),
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount((uint32_t)scissor.size());
			viewportInfo.setPScissors(scissor.data());

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info[2];
			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(glm::vec3))
			};

			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(0),
			};

			vertex_input_info[0].setVertexBindingDescriptionCount(vertex_input_binding.size());
			vertex_input_info[0].setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info[0].setVertexAttributeDescriptionCount(vertex_input_attribute.size());
			vertex_input_info[0].setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(3)
				.setPStages(&m_shader_info.data()[SHADER_VERTEX_FLOOR])
				.setPVertexInputState(&vertex_input_info[0])
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(2)
				.setPStages(&m_shader_info.data()[SHADER_VERTEX_FLOOR_EX])
				.setPVertexInputState(&vertex_input_info[1])
				.setPInputAssemblyState(&assembly_info[1])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			std::copy(std::make_move_iterator(pipelines.begin()), std::make_move_iterator(pipelines.end()), m_pipeline.begin());

		}

		vk::CommandBufferAllocateInfo cmd_info;
		cmd_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_info.level = vk::CommandBufferLevel::ePrimary;
		m_cmd = std::move(context->m_device->allocateCommandBuffersUnique(cmd_info));

		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		for (size_t i = 0; i < m_cmd.size(); i++)
		{
			auto& cmd = m_cmd[i];
			cmd->begin(begin_info);

			{
				vk::RenderPassBeginInfo begin_render_Info = vk::RenderPassBeginInfo()
					.setRenderPass(m_render_pass.get())
					.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(context->m_window->getClientSize().x, context->m_window->getClientSize().y)))
					.setFramebuffer(m_framebuffer[i].get());
				cmd->beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

				cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_FLOOR_EX].get());
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 2, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_SCENE), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 3, sLightSystem::Order().getDescriptorSet(sLightSystem::DESCRIPTOR_SET_LIGHT), {});
				cmd->draw(4, 1, 0, 0);

				cmd->endRenderPass();
			}
			cmd->end();
		}

		VoxelInfo info;
		info.u_area_min = vec4(0.f, 0.f, 0.f, 1.f);
		info.u_area_max = vec4(500.f, 20.f, 500.f, 1.f);
		info.u_cell_num = uvec4(128, 4, 128, 1);
		info.u_cell_size = (info.u_area_max - info.u_area_min) / vec4(info.u_cell_num);
		m_voxelize_pipeline.setup(context, info);

	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer)
	{
		return m_cmd[sGlobal::Order().getCurrentFrame()].get();
	}


	VoxelPipeline& getVoxel() { return m_voxelize_pipeline; }

};

