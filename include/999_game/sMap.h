#if 0
// 今の
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

struct DrawDescriptor
{
	uint32_t instance_num;

	btr::BufferMemoryEx<uint32_t> m_instance_index_map;
	btr::BufferMemoryEx<uint32_t> m_visible;
	btr::BufferMemoryEx<vk::DrawIndexedIndirectCommand> m_draw_cmd;
	btr::BufferMemoryEx<mat4> m_world;

	void updateDescriptor(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set)
	{
		vk::DescriptorBufferInfo storages[] = {
			m_world.getInfo(),
			m_visible.getInfo(),
			m_draw_cmd.getInfo(),
			m_instance_index_map.getInfo(),
		};
		vk::WriteDescriptorSet write_desc[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(descriptor_set),
		};
		context->m_device.updateDescriptorSets(array_length(write_desc), write_desc, 0, nullptr);

	}
	static LayoutDescriptor GetLayout()
	{
		LayoutDescriptor desc;
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		desc.binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
		};
		desc.m_set_num = 10;
		return desc;
	}
};

struct sMap : public Singleton<sMap>
{
	struct DrawResource
	{
		btr::BufferMemoryEx<vk::DrawIndirectCommand> m_draw_indirect;
		btr::BufferMemoryEx<uint32_t> m_instance_index_map;
	};

	enum
	{
		SHADER_VERTEX_FLOOR,
		SHADER_GEOMETRY_FLOOR,
		SHADER_FRAGMENT_FLOOR,
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
		PIPELINE_NUM,
	};

	std::shared_ptr<RenderBackbufferModule> m_render_pass;
	std::vector<vk::UniqueCommandBuffer> m_cmd;

	std::shared_ptr<DrawResource> m_draw_resource;
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
			m_render_pass = std::make_shared<RenderBackbufferModule>(context);
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
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.get(), path + shader_info[i].name);
				m_shader_info[i].setModule(m_shader_module[i].get());
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		{
			auto resource = std::make_shared<DrawResource>();
			uint32_t num = 1024;
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(vk::DrawIndirectCommand);
				resource->m_draw_indirect = context->m_vertex_memory.allocateMemory<vk::DrawIndirectCommand>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<vk::DrawIndirectCommand>(desc);
				auto* indirect_cmd = staging.getMappedPtr();
				indirect_cmd->instanceCount = 1024;
				indirect_cmd->firstVertex = 0;
				indirect_cmd->setVertexCount(14);
				indirect_cmd->firstInstance = 0;

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(resource->m_draw_indirect.getInfo().offset);
				copy.setSize(desc.size);
				cmd->copyBuffer(staging.getInfo().buffer, resource->m_draw_indirect.getInfo().buffer, copy);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t) * num;
				auto buffer = context->m_storage_memory.allocateMemory<uint32_t>(desc);

				resource->m_instance_index_map = buffer;
			}
			m_draw_resource = resource;
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
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		vk::Extent2D size = m_render_pass->getResolution();
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::ePointList),
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

			vk::PipelineVertexInputStateCreateInfo vertex_input_info[1];

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
				.setRenderPass(m_render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
			std::copy(std::make_move_iterator(pipelines.begin()), std::make_move_iterator(pipelines.end()), m_pipeline.begin());

		}

		vk::CommandBufferAllocateInfo cmd_info;
		cmd_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_info.level = vk::CommandBufferLevel::ePrimary;
		m_cmd = std::move(context->m_device.allocateCommandBuffersUnique(cmd_info));

		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		for (size_t i = 0; i < m_cmd.size(); i++)
		{
			auto& cmd = m_cmd[i];
			cmd->begin(begin_info);

			{
				vk::RenderPassBeginInfo begin_render_Info = vk::RenderPassBeginInfo()
					.setRenderPass(m_render_pass->getRenderPass())
					.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(context->m_window->getClientSize().x, context->m_window->getClientSize().y)))
					.setFramebuffer(m_render_pass->getFramebuffer(i));
				cmd->beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

				cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_FLOOR].get());
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 2, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_SCENE), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 3, sLightSystem::Order().getDescriptorSet(sLightSystem::DESCRIPTOR_SET_LIGHT), {});
				auto num = sScene::Order().m_map_info_cpu.m_descriptor[0].m_cell_num.x *sScene::Order().m_map_info_cpu.m_descriptor[0].m_cell_num.y;
				cmd->draw(1, num, 0, 0);

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

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context)
	{
		return m_cmd[sGlobal::Order().getCurrentFrame()].get();
	}


	VoxelPipeline& getVoxel() { return m_voxelize_pipeline; }

};
#else
// 昔の
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


struct sMap : public Singleton<sMap>
{
	enum
	{
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
		PIPELINE_NUM,
	};

	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;
	vk::UniqueCommandBuffer m_cmd;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

//	VoxelPipeline m_voxelize_pipeline;

	void setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_render_target = render_target;
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
			vk::AttachmentReference depth_ref;
			depth_ref.setAttachment(1);
			depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);
			subpass.setPDepthStencilAttachment(&depth_ref);

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
				// depth
				vk::AttachmentDescription()
				.setFormat(render_target->m_depth_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);
		}

		{
			vk::ImageView view[] = {
				render_target->m_view,
				render_target->m_depth_view,
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

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}
			shader_info[] =
			{
				{ "FloorRenderEx.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "FloorRenderEx.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device, path + shader_info[i].name);
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
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		vk::Extent2D size = render_target->m_resolution;
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

			vk::PipelineShaderStageCreateInfo shader_info[] = 
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader_module[SHADER_VERTEX_FLOOR_EX].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader_module[SHADER_FRAGMENT_FLOOR_EX].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment)
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
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
			auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
			std::copy(std::make_move_iterator(pipelines.begin()), std::make_move_iterator(pipelines.end()), m_pipeline.begin());

		}

		vk::CommandBufferAllocateInfo cmd_info;
		cmd_info.commandBufferCount = 1;
		cmd_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_info.level = vk::CommandBufferLevel::ePrimary;
		m_cmd = std::move(context->m_device.allocateCommandBuffersUnique(cmd_info)[0]);

		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		m_cmd->begin(begin_info);
		{
			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), render_target->m_resolution));
			begin_render_Info.setFramebuffer(m_framebuffer.get());
			m_cmd->beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			m_cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_FLOOR].get());
			m_cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
			m_cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
			m_cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 2, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_SCENE), {});
			m_cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 3, sLightSystem::Order().getDescriptorSet(sLightSystem::DESCRIPTOR_SET_LIGHT), {});
			m_cmd->draw(4, 1, 0, 0);

			m_cmd->endRenderPass();
		}
		m_cmd->end();

	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer)
	{
		return m_cmd.get();
	}

};
#endif