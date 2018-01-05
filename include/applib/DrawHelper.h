#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>
#include <applib/Geometry.h>
#include <applib/Utility.h>
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
	std::shared_ptr<RenderPassModule> m_render_pass;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	std::array<btr::BufferMemory, PrimitiveType_MAX> m_mesh_vertex;
	std::array<btr::BufferMemory, PrimitiveType_MAX> m_mesh_index;
	std::array<uint32_t, PrimitiveType_MAX> m_mesh_index_num;
	std::array<std::array<AppendBuffer<DrawCommand, 1024>, PrimitiveType_MAX>, 2> m_draw_cmd;
	std::array<AppendBuffer<GeometryVertex, 1024>, 2> m_draw_dynamic_vertex;
	

	DrawHelper()
	{

	}
	void setup(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);


		{
			std::vector<vec3> v;
			std::vector<uvec3> i;
			std::tie(v, i) = Geometry::MakeBox();
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(v);
				m_mesh_vertex[Box] = context->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), v.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_vertex[Box].getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_vertex[Box].getBufferInfo().buffer, copy);
			}

			{
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(i);
				m_mesh_index[Box] = context->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), i.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_index[Box].getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_index[Box].getBufferInfo().buffer, copy);
			}
			m_mesh_index_num[Box] = i.size();

		}

		{
			std::vector<vec3> v;
			std::vector<uvec3> i;
			std::tie(v, i) = Geometry::MakeSphere();
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(v);
				m_mesh_vertex[SPHERE] = context->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), v.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_vertex[SPHERE].getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_vertex[SPHERE].getBufferInfo().buffer, copy);
			}

			{
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(i);
				m_mesh_index[SPHERE] = context->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), i.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(m_mesh_index[SPHERE].getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getBufferInfo().buffer, m_mesh_index[SPHERE].getBufferInfo().buffer, copy);
			}
			m_mesh_index_num[SPHERE] = i.size() * 3;
		}
		{
			std::vector<vk::BufferMemoryBarrier> barrier =
			{
				m_mesh_index[Box].makeMemoryBarrier(vk::AccessFlagBits::eIndexRead),
				m_mesh_index[SPHERE].makeMemoryBarrier(vk::AccessFlagBits::eIndexRead),
			};
			barrier[0].setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			barrier[1].setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput, {}, {}, barrier, {});
		}

		{
			m_render_pass = context->m_window->getRenderBackbufferPass();
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

			std::string path = btr::getResourceLibPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
				m_shader_info[i].setModule(m_shader_module[i].get());
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

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
			pipeline_layout_info.setSetLayoutCount((uint32_t)layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(array_length(constant));
			pipeline_layout_info.setPPushConstantRanges(constant);
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// setup pipeline
		vk::Extent3D size;
		size.setWidth(context->m_window->getClientSize().x);
		size.setHeight(context->m_window->getClientSize().y);
		size.setDepth(1);

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
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE].get())
				.setRenderPass(m_render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PIPELINE_DRAW_PRIMITIVE] = std::move(pipelines[0]);

		}

	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer)
	{
		auto cmd = executer->m_cmd_pool->allocCmdOnetime(0);

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass->getRenderPass());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(640, 480)));
		begin_render_Info.setFramebuffer(m_render_pass->getFramebuffer(executer->getGPUFrame()));
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_PRIMITIVE].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE].get(), 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

		auto& draw_cmd = m_draw_cmd[sGlobal::Order().getGPUIndex()];
		for (size_t i = 0; i < draw_cmd.size(); i++)
		{
			auto& cmd_list = draw_cmd[i];
			if (cmd_list.empty()) {
				continue;
			}

			cmd.bindVertexBuffers(0, m_mesh_vertex[i].getBufferInfo().buffer, m_mesh_vertex[i].getBufferInfo().offset);
			cmd.bindIndexBuffer(m_mesh_index[i].getBufferInfo().buffer, m_mesh_index[i].getBufferInfo().offset, vk::IndexType::eUint32);

			auto cmd_data = cmd_list.get();
			for (auto& dcmd : cmd_data)
			{
				cmd.pushConstants<mat4>(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_PRIMITIVE].get(), vk::ShaderStageFlagBits::eVertex, 0, dcmd.world);
				cmd.drawIndexed(m_mesh_index_num[i], 1, 0, 0, 0);
			}
		}

		cmd.endRenderPass();
		cmd.end();

		return cmd;
	}

	void drawOrder(PrimitiveType type, const DrawCommand& cmd)
	{
		m_draw_cmd[sGlobal::Order().getCPUIndex()][type].push(&cmd, 1);
	}
};