#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/loader.h>
#include <btrlib/cCamera.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>
#include <applib/Geometry.h>
#include <applib/sCameraManager.h>

struct DrawCommand
{
	mat4 world;
};
struct DrawHelper : public Singleton<DrawHelper>
{
	friend Singleton<DrawHelper>;

	enum {
		CMD_SIZE = 256,
	};
	enum SHADER
	{
		SHADER_VERTEX_PRIMITIVE,
		SHADER_FRAGMENT_PRIMITIVE,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_DRAW_PRIMITIVE,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_PRIMITIVE,
		PIPELINE_NUM,
	};

	enum PrimitiveType
	{
		Box,
		SPHERE,
		PrimitiveType_MAX,
	};
	vk::RenderPass m_render_pass;
	std::vector<vk::Framebuffer> m_framebuffer;

	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::Pipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::DescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	std::array<btr::AllocatedMemory, PrimitiveType_MAX> m_mesh_vertex;
	std::array<btr::AllocatedMemory, PrimitiveType_MAX> m_mesh_index;
	std::array<uint32_t, PrimitiveType_MAX> m_mesh_index_num;
	std::vector<std::array<std::array<std::vector<DrawCommand>, PrimitiveType_MAX>, 2>> m_draw_cmd;

	DrawHelper()
	{

	}
	void setup(std::shared_ptr<btr::Loader>& loader)
	{
		m_draw_cmd.resize(sGlobal::Order().getThreadPool().getThreadNum()+1);
		{
			// レンダーパス
			std::vector<vk::AttachmentReference> colorRef =
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
			subpass.setColorAttachmentCount((uint32_t)colorRef.size());
			subpass.setPColorAttachments(colorRef.data());
			subpass.setPDepthStencilAttachment(&depth_ref);
			// render pass
			std::vector<vk::AttachmentDescription> attachDescription = 
			{
				vk::AttachmentDescription()
				.setFormat(loader->m_window->getSwapchain().m_surface_format.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eClear)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				vk::AttachmentDescription()
				.setFormat(vk::Format::eD32Sfloat)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eClear)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),

			};
			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(attachDescription.size())
				.setPAttachments(attachDescription.data())
				.setSubpassCount(1)
				.setPSubpasses(&subpass);

			m_render_pass = loader->m_device->createRenderPass(renderpass_info);

			// フレームバッファ
			m_framebuffer.resize(loader->m_window->getSwapchain().getSwapchainNum());
			{
				std::vector<vk::ImageView> view(2);

				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass);
				framebuffer_info.setAttachmentCount((uint32_t)view.size());
				framebuffer_info.setPAttachments(view.data());
				framebuffer_info.setWidth(loader->m_window->getClientSize().x);
				framebuffer_info.setHeight(loader->m_window->getClientSize().y);
				framebuffer_info.setLayers(1);

				for (size_t i = 0; i < m_framebuffer.size(); i++) {
					view[0] = loader->m_window->getSwapchain().m_backbuffer[i].m_view;
					view[1] = loader->m_window->getSwapchain().m_depth.m_view;
					m_framebuffer[i] = loader->m_device->createFramebuffer(framebuffer_info);
				}
			}

		}


		{
			std::vector<vec3> v;
			std::vector<uvec3> i;
			std::tie(v, i) = Geometry::MakeBox();
			{
				btr::BufferMemory::Descriptor desc;
				desc.size = vector_sizeof(v);
				m_mesh_vertex[Box] = loader->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = loader->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), v.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_vertex[Box].getBufferInfo().offset);
				copy.setSize(desc.size);
				loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_vertex[Box].getBufferInfo().buffer, copy);
			}

			{
				btr::BufferMemory::Descriptor desc;
				desc.size = vector_sizeof(i);
				m_mesh_index[Box] = loader->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = loader->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), i.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_index[Box].getBufferInfo().offset);
				copy.setSize(desc.size);
				loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_index[Box].getBufferInfo().buffer, copy);
			}
			m_mesh_index_num[Box] = i.size();

		}

		{
			std::vector<vec3> v;
			std::vector<uvec3> i;
			std::tie(v, i) = Geometry::MakeSphere();
			{
				btr::BufferMemory::Descriptor desc;
				desc.size = vector_sizeof(v);
				m_mesh_vertex[SPHERE] = loader->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = loader->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), v.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_vertex[SPHERE].getBufferInfo().offset);
				copy.setSize(desc.size);
				loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_vertex[SPHERE].getBufferInfo().buffer, copy);
			}

			{
				btr::BufferMemory::Descriptor desc;
				desc.size = vector_sizeof(i);
				m_mesh_index[SPHERE] = loader->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = loader->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), i.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_index[SPHERE].getBufferInfo().offset);
				copy.setSize(desc.size);
				loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_index[SPHERE].getBufferInfo().buffer, copy);
			}
			m_mesh_index_num[SPHERE] = i.size()*3;
		}
		{
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				m_mesh_vertex[Box].makeMemoryBarrier(vk::AccessFlagBits::eVertexAttributeRead),
				m_mesh_index[Box].makeMemoryBarrier(vk::AccessFlagBits::eIndexRead),
				m_mesh_vertex[SPHERE].makeMemoryBarrier(vk::AccessFlagBits::eVertexAttributeRead),
				m_mesh_index[SPHERE].makeMemoryBarrier(vk::AccessFlagBits::eIndexRead),
			};
			loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput, {}, {}, barrier, {});
		}
		
		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "DrawPrimitive.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "DrawPrimitive.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_info[i].name));
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		// setup descriptor_set_layout
		// setup descriptor_set

		// setup pipeline_layout
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			vk::PushConstantRange constant[] = {
				vk::PushConstantRange()
				.setSize(64)
				.setStageFlags(vk::ShaderStageFlagBits::eVertex)
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(array_length(constant));
			pipeline_layout_info.setPPushConstantRanges(constant);
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE] = loader->m_device->createPipelineLayout(pipeline_layout_info);
		}

		// setup pipeline
		vk::Extent3D size;
		size.setWidth(640);
		size.setHeight(480);
		size.setDepth(1);
		// pipeline
		// 		{
		// 			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		// 			{
		// 				vk::ComputePipelineCreateInfo()
		// 				.setStage(m_shader_info[SHADER_COMPUTE_MAP_UPDATE])
		// 				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR]),
		// 			};
		// 			auto pipelines = loader->m_device->createComputePipelines(loader->m_cache, compute_pipeline_info);
		// 			m_pipeline[PIPELINE_COMPUTE_MAP] = pipelines[0];
		// 		}
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList),
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
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_TRUE)
				.setSrcColorBlendFactor(vk::BlendFactor::eOne)
				.setDstColorBlendFactor(vk::BlendFactor::eDstAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::VertexInputAttributeDescription attr[] =
			{
				vk::VertexInputAttributeDescription().setLocation(0).setOffset(0).setBinding(0).setFormat(vk::Format::eR32G32B32A32Sfloat)
			};
			vk::VertexInputBindingDescription binding[] =
			{
				vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(12)
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexAttributeDescriptionCount(array_length(attr));
			vertex_input_info.setPVertexAttributeDescriptions(attr);
			vertex_input_info.setVertexBindingDescriptionCount(array_length(binding));
			vertex_input_info.setPVertexBindingDescriptions(binding);

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				m_shader_info[SHADER_VERTEX_PRIMITIVE],
				m_shader_info[SHADER_FRAGMENT_PRIMITIVE],
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE])
				.setRenderPass(loader->m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = loader->m_device->createGraphicsPipelines(loader->m_cache, graphics_pipeline_info);
			m_pipeline[PIPELINE_DRAW_PRIMITIVE] = pipelines[0];

		}

	}

	vk::CommandBuffer draw()
	{
		auto device = sGlobal::Order().getGPU(0).getDevice();
		auto cmd = sThreadLocal::Order().getCmdOnetime(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

		std::vector<vk::ClearValue> clearValue = {
			vk::ClearValue().setColor(vk::ClearColorValue(std::array<float, 4>{0.3f, 0.3f, 0.8f, 1.f})),
			vk::ClearValue().setDepthStencil(vk::ClearDepthStencilValue(1.f)),
		};
		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass);
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(640, 480)));
		begin_render_Info.setFramebuffer(m_framebuffer[sGlobal::Order().getCurrentFrame()]);
		begin_render_Info.setClearValueCount(clearValue.size());
		begin_render_Info.setPClearValues(clearValue.data());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_PRIMITIVE]);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE], 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

		for (auto& cmd_per_thread : m_draw_cmd)
		{
			auto& draw_cmd = cmd_per_thread[sGlobal::Order().getGPUIndex()];
			for (size_t i = 0; i < draw_cmd.size(); i++)
			{
				auto& cmd_list = draw_cmd[i];
				if (cmd_list.empty()) {
					continue;
				}

				cmd.bindVertexBuffers(0, m_mesh_vertex[i].getBufferInfo().buffer, m_mesh_vertex[i].getBufferInfo().offset);
				cmd.bindIndexBuffer(m_mesh_index[i].getBufferInfo().buffer, m_mesh_index[i].getBufferInfo().offset, vk::IndexType::eUint32);

				for (auto& dcmd : cmd_list)
				{
					cmd.pushConstants<mat4>(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE], vk::ShaderStageFlagBits::eVertex, 0, dcmd.world);
					cmd.drawIndexed(m_mesh_index_num[i], 1, 0, 0, 0);
				}
				cmd_list.clear();
			}
		}

		cmd.endRenderPass();
		cmd.end();

		return cmd;
	}

	void drawOrder(PrimitiveType type, const DrawCommand& cmd)
	{
		m_draw_cmd[sThreadLocal::Order().getThreadIndex()][sGlobal::Order().getCPUIndex()][type].push_back(cmd);
	}
};