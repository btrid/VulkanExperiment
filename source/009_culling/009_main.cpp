#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <btrlib/Context.h>

#include <applib/Geometry.h>
#include <applib/sCameraManager.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct InstancingDescriptors
{
	btr::BufferMemoryEx<uint32_t> m_instance_index_map;
	btr::BufferMemoryEx<uint32_t> m_visible;
	btr::BufferMemoryEx<vk::DrawIndexedIndirectCommand> m_draw_cmd;
	btr::BufferMemoryEx<mat4> m_world;
};

struct InstancingDescriptorLayout : public DescriptorModule
{
	InstancingDescriptorLayout(const std::shared_ptr<btr::Context>& context)
	{
		m_context = context;
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		std::vector<vk::DescriptorSetLayoutBinding> binding =
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
		m_descriptor_pool = btr::Descriptor::createDescriptorPool(context, binding, 100);
		m_descriptor_set_layout = btr::Descriptor::createDescriptorSetLayout(context, binding);

	}

	vk::UniqueDescriptorSet allocateDescriptorSet(InstancingDescriptors& descriptors)
	{
		vk::UniqueDescriptorSet descriptor_set = btr::Descriptor::allocateDescriptorSet(m_context, m_descriptor_pool.get(), m_descriptor_set_layout.get());
		vk::DescriptorBufferInfo storages[] = {
			descriptors.m_world.getInfo(),
			descriptors.m_visible.getInfo(),
			descriptors.m_draw_cmd.getInfo(),
			descriptors.m_instance_index_map.getInfo(),
		};
		vk::WriteDescriptorSet write_desc[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(descriptor_set.get()),
		};
		m_context->m_device.updateDescriptorSets(array_length(write_desc), write_desc, 0, nullptr);
		return descriptor_set;
	}

	std::shared_ptr<btr::Context> m_context;
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorPool m_descriptor_pool;

	vk::DescriptorPool getPool()const override { return m_descriptor_pool.get(); }
	vk::DescriptorSetLayout getLayout()const override { return m_descriptor_set_layout.get(); }
};


struct CullingTest
{
	struct ResourceVertex //: IDepthRender
	{
		btr::BufferMemoryEx<vec3> m_vertex;
		btr::BufferMemoryEx<uvec3> m_index;

		btr::BufferMemoryEx<vk::DrawIndexedIndirectCommand> m_draw_indirect;

		btr::BufferMemoryEx<mat4> m_world;
		btr::BufferMemoryEx<uint32_t> m_instance_index_map;

		InstancingDescriptors m_descriptors;
		vk::UniqueDescriptorSet m_descriptor_set;
	};
	struct Renderer
	{
		std::vector<std::shared_ptr<ResourceVertex>> m_renders;
	};

	enum Shader
	{
		SHADER_VERT_WRITE_DEPTH,
		SHADER_FRAG_WRITE_DEPTH,
		SHADER_VERT_OCCULSION_TEST,
		SHADER_FRAG_OCCULSION_TEST,
		SHADER_VERT_RENDER,
		SHADER_GEOM_RENDER,
		SHADER_FRAG_RENDER,
		SHADER_COMP_VISIBLE,
		SHADER_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_COMPUTE,
		PIPELINE_LAYOUT_DRAW,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_GRAPHICS_WRITE_DEPTH,
		PIPELINE_GRAPHICS_OCCULSION_TEST,
		PIPELINE_GRAPHICS_RENDER,
		PIPELINE_COMPUTE_VISIBLE,
		PIPELINE_NUM,
	};

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::shared_ptr<InstancingDescriptorLayout> m_occlusion_descriptor_layout;

	CullingTest(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		m_occlusion_descriptor_layout = std::make_shared<InstancingDescriptorLayout>(context);

		{
			std::shared_ptr<ResourceVertex> resource = std::make_shared<ResourceVertex>();
			std::vector<vec3> v;
			std::vector<uvec3> i;
			int num = 1000;
			std::tie(v, i) = Geometry::MakeSphere();
			{

				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(v);
				resource->m_vertex = context->m_vertex_memory.allocateMemory<vec3>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<vec3>(desc);
				memcpy(staging.getMappedPtr(), v.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(resource->m_vertex.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, resource->m_vertex.getInfo().buffer, copy);
			}

			{
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(i);
				resource->m_index = context->m_vertex_memory.allocateMemory<uvec3>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<uvec3>(desc);
				memcpy(staging.getMappedPtr(), i.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(resource->m_index.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, resource->m_index.getInfo().buffer, copy);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(vk::DrawIndexedIndirectCommand);
				resource->m_draw_indirect = context->m_vertex_memory.allocateMemory<vk::DrawIndexedIndirectCommand>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<vk::DrawIndexedIndirectCommand>(desc);
				auto* indirect_cmd = staging.getMappedPtr();
				indirect_cmd->indexCount = i.size() * 3;
				indirect_cmd->instanceCount = num;
				indirect_cmd->firstIndex = 0;
				indirect_cmd->vertexOffset = 0;
				indirect_cmd->firstInstance = 0;

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(resource->m_draw_indirect.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, resource->m_draw_indirect.getInfo().buffer, copy);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(mat4) * num;
				auto buffer = context->m_storage_memory.allocateMemory<mat4>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<mat4>(desc);
				auto* world = staging.getMappedPtr();
				for (int i = 0; i < num; i++)
				{
					auto translate = glm::translate(glm::ballRand(1000.f));
					auto scale = glm::scale(translate, glm::linearRand(vec3(3.f), vec3(30.f)));
					world[i] = scale;
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(buffer.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy);
				resource->m_world = buffer;
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t) * num;
				auto buffer = context->m_storage_memory.allocateMemory<uint32_t>(desc);

				resource->m_instance_index_map = buffer;
			}

			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t) * num;
				resource->m_descriptors.m_visible = context->m_storage_memory.allocateMemory<uint32_t>(desc);
				resource->m_descriptors.m_draw_cmd = resource->m_draw_indirect;
				resource->m_descriptors.m_instance_index_map = resource->m_instance_index_map;
				resource->m_descriptors.m_world = resource->m_world;
				resource->m_descriptor_set = m_occlusion_descriptor_layout->allocateDescriptorSet(resource->m_descriptors);
			}

			m_culling_target = resource;
		}

		{
			std::shared_ptr<ResourceVertex> resource = std::make_shared<ResourceVertex>();
			std::vector<vec3> v;
			std::vector<uvec3> i;
			std::tie(v, i) = Geometry::MakeBox(vec3(500.f, 500.f, 1.f));
			int num = 1;
			{

				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(v);
				resource->m_vertex = context->m_vertex_memory.allocateMemory<vec3>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<vec3>(desc);
				memcpy(staging.getMappedPtr(), v.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(resource->m_vertex.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, resource->m_vertex.getInfo().buffer, copy);
			}

			{
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(i);
				resource->m_index = context->m_vertex_memory.allocateMemory<uvec3>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<uvec3>(desc);
				memcpy(staging.getMappedPtr(), i.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(resource->m_index.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, resource->m_index.getInfo().buffer, copy);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(vk::DrawIndexedIndirectCommand);
				resource->m_draw_indirect = context->m_vertex_memory.allocateMemory<vk::DrawIndexedIndirectCommand>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<vk::DrawIndexedIndirectCommand>(desc);
				auto* indirect_cmd = staging.getMappedPtr();
				indirect_cmd->indexCount = i.size() * 3;
				indirect_cmd->instanceCount = num;
				indirect_cmd->firstIndex = 0;
				indirect_cmd->vertexOffset = 0;
				indirect_cmd->firstInstance = 0;

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(resource->m_draw_indirect.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, resource->m_draw_indirect.getInfo().buffer, copy);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(mat4) * num;
				auto buffer = context->m_storage_memory.allocateMemory<mat4>(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory<mat4>(desc);
				auto* world = staging.getMappedPtr();
				for (int i = 0; i < num; i++)
				{
					world[i] = mat4(1.f);
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(buffer.getInfo().offset);
				copy.setSize(desc.size);
				cmd.copyBuffer(staging.getInfo().buffer, buffer.getInfo().buffer, copy);
				resource->m_world = buffer;
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t) * num;
				auto buffer = context->m_storage_memory.allocateMemory<uint32_t>(desc);

				resource->m_instance_index_map = buffer;
			}
			{


				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t) * num;
				resource->m_descriptors.m_visible = context->m_storage_memory.allocateMemory<uint32_t>(desc);
				resource->m_descriptors.m_draw_cmd = resource->m_draw_indirect;
				resource->m_descriptors.m_instance_index_map = resource->m_instance_index_map;
				resource->m_descriptors.m_world = resource->m_world;

				resource->m_descriptor_set = m_occlusion_descriptor_layout->allocateDescriptorSet(resource->m_descriptors);
			}


			m_occluder = resource;
		}

		{
			m_render_pass = context->m_window->getRenderBackbufferPass();
			auto& device = context->m_device;
			// レンダーパス
			{
				// sub pass
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(0);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setPDepthStencilAttachment(&depth_ref);

				vk::AttachmentDescription attach_description[] = {
					vk::AttachmentDescription()
					.setFormat(app::g_app_instance->m_window->m_depth_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);
				renderpass_info.setAttachmentCount(array_length(attach_description));
				renderpass_info.setPAttachments(attach_description);

				m_occlusion_test_pass = context->m_device.createRenderPassUnique(renderpass_info);
			}
			{
				vk::ImageView view[] = {
					app::g_app_instance->m_window->m_depth_view.get(),
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_occlusion_test_pass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(context->m_window->getClientSize().x);
				framebuffer_info.setHeight(context->m_window->getClientSize().y);
				framebuffer_info.setLayers(1);

				m_occlusion_test_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}

		}
		{
			// setup shader
			{
				struct
				{
					const char* name;
					vk::ShaderStageFlagBits stage;
				}shader_info[] =
				{
					{ "WriteDepth.vert.spv",vk::ShaderStageFlagBits::eVertex },
					{ "WriteDepth.frag.spv",vk::ShaderStageFlagBits::eFragment },
					{ "OcclusionTest.vert.spv",vk::ShaderStageFlagBits::eVertex },
					{ "OcclusionTest.frag.spv",vk::ShaderStageFlagBits::eFragment },
					{ "Render.vert.spv",vk::ShaderStageFlagBits::eVertex },
					{ "Render.geom.spv",vk::ShaderStageFlagBits::eGeometry },
					{ "Render.frag.spv",vk::ShaderStageFlagBits::eFragment },
					{ "CalcVisibleGeometry.comp.spv",vk::ShaderStageFlagBits::eCompute },
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

			// setup pipeline_layout
			{
				std::vector<vk::DescriptorSetLayout> layouts =
				{
					m_occlusion_descriptor_layout->getLayout(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
				m_pipeline_layout[PIPELINE_LAYOUT_DRAW] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				std::vector<vk::DescriptorSetLayout> layouts =
				{
					m_occlusion_descriptor_layout->getLayout(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
				m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

			{
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
					vk::ComputePipelineCreateInfo()
					.setStage(m_shader_info[SHADER_COMP_VISIBLE])
					.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get()),
				};

				auto p = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
				m_pipeline[PIPELINE_COMPUTE_VISIBLE] = std::move(p[0]);

			}
			// setup pipeline
			vk::Extent3D size;
			size.setWidth(m_render_pass->getResolution().width);
			size.setHeight(m_render_pass->getResolution().height);
			size.setDepth(1);

			{
				// assembly
				vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
				{
					vk::PipelineInputAssemblyStateCreateInfo()
					.setPrimitiveRestartEnable(VK_FALSE)
					.setTopology(vk::PrimitiveTopology::eTriangleList),
				};

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
				vk::PipelineColorBlendStateCreateInfo no_blend_info;

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

				{
					// viewport
					std::vector<vk::Viewport> viewport =
					{
						vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f)
					};
					std::vector<vk::Rect2D> scissor =
					{
						vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
					};
					vk::PipelineViewportStateCreateInfo viewportInfo;
					viewportInfo.setViewportCount((uint32_t)viewport.size());
					viewportInfo.setPViewports(viewport.data());
					viewportInfo.setScissorCount((uint32_t)scissor.size());
					viewportInfo.setPScissors(scissor.data());
					vk::PipelineShaderStageCreateInfo shader_info[] = {
						m_shader_info[SHADER_VERT_WRITE_DEPTH],
						m_shader_info[SHADER_FRAG_WRITE_DEPTH],
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
						.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get())
						.setRenderPass(m_occlusion_test_pass.get())
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&no_blend_info),
					};
					auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
					m_pipeline[PIPELINE_GRAPHICS_WRITE_DEPTH] = std::move(pipelines[0]);

				}

				{
					// viewport
					std::vector<vk::Viewport> viewport =
					{
						vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f)
					};
					std::vector<vk::Rect2D> scissor =
					{
						vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
					};
					vk::PipelineViewportStateCreateInfo viewportInfo;
					viewportInfo.setViewportCount((uint32_t)viewport.size());
					viewportInfo.setPViewports(viewport.data());
					viewportInfo.setScissorCount((uint32_t)scissor.size());
					viewportInfo.setPScissors(scissor.data());

					vk::PipelineShaderStageCreateInfo shader_info[] = {
						m_shader_info[SHADER_VERT_OCCULSION_TEST],
						m_shader_info[SHADER_FRAG_OCCULSION_TEST],
					};
					vk::PipelineColorBlendStateCreateInfo no_blend_info;

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
						.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get())
						.setRenderPass(m_occlusion_test_pass.get())
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&no_blend_info),
					};

					auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
					m_pipeline[PIPELINE_GRAPHICS_OCCULSION_TEST] = std::move(pipelines[0]);

				}

				{
					// viewport
					std::vector<vk::Viewport> viewport =
					{
						vk::Viewport(0.f, 0.f, 800.f, (float)size.height, 0.f, 1.f),
						vk::Viewport(800.f, 0.f, 800.f, (float)size.height, 0.f, 1.f),
					};
					std::vector<vk::Rect2D> scissor =
					{
						vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(800, size.height)),
						vk::Rect2D(vk::Offset2D(800, 0), vk::Extent2D(800, size.height)),
					};
					vk::PipelineColorBlendStateCreateInfo blend_info;
					blend_info.setAttachmentCount(blend_state.size());
					blend_info.setPAttachments(blend_state.data());

					vk::PipelineViewportStateCreateInfo viewportInfo;
					viewportInfo.setViewportCount((uint32_t)viewport.size());
					viewportInfo.setPViewports(viewport.data());
					viewportInfo.setScissorCount((uint32_t)scissor.size());
					viewportInfo.setPScissors(scissor.data());
					vk::PipelineShaderStageCreateInfo shader_info[] = {
						m_shader_info[SHADER_VERT_RENDER],
						m_shader_info[SHADER_GEOM_RENDER],
						m_shader_info[SHADER_FRAG_RENDER],
					};

					vk::PipelineDepthStencilStateCreateInfo no_depth_stencil_info;
					depth_stencil_info.setDepthTestEnable(VK_TRUE);
					depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);

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
						.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get())
						.setRenderPass(m_render_pass->getRenderPass())
						.setPDepthStencilState(&no_depth_stencil_info)
						.setPColorBlendState(&blend_info),
					};
					auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
					m_pipeline[PIPELINE_GRAPHICS_RENDER] = std::move(pipelines[0]);

				}

			}

		}
	}
	vk::CommandBuffer draw(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		{
			{
				auto to_transfer = m_culling_target->m_draw_indirect.makeMemoryBarrier();
				to_transfer.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
				to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.updateBuffer<uint32_t>(m_culling_target->m_draw_indirect.getInfo().buffer, m_culling_target->m_descriptors.m_draw_cmd.getInfo().offset + offsetof(vk::DrawIndexedIndirectCommand, instanceCount), 1000u);

				auto to_read = m_culling_target->m_descriptors.m_draw_cmd.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, to_read, {});
			}

			{

				auto to_transfer = m_culling_target->m_descriptors.m_visible.makeMemoryBarrier();
				to_transfer.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.fillBuffer(m_culling_target->m_descriptors.m_visible.getInfo().buffer, m_culling_target->m_descriptors.m_visible.getInfo().offset, m_culling_target->m_descriptors.m_visible.getInfo().range, 0u);

				auto to_write = m_culling_target->m_descriptors.m_visible.makeMemoryBarrier();
				to_write.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_write.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, to_write, {});

			}
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_occlusion_test_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()));
		begin_render_Info.setFramebuffer(m_occlusion_test_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		{
			// depth
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_WRITE_DEPTH].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_occluder->m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

			cmd.bindVertexBuffers(0, m_occluder->m_vertex.getInfo().buffer, m_occluder->m_vertex.getInfo().offset);
			cmd.bindIndexBuffer(m_occluder->m_index.getInfo().buffer, m_occluder->m_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.drawIndexedIndirect(m_occluder->m_draw_indirect.getInfo().buffer, m_occluder->m_draw_indirect.getInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));
		}

		{
			// culling visible test
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_OCCULSION_TEST].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_culling_target->m_descriptor_set.get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

			cmd.bindVertexBuffers(0, m_culling_target->m_vertex.getInfo().buffer, m_culling_target->m_vertex.getInfo().offset);
			cmd.bindIndexBuffer(m_culling_target->m_index.getInfo().buffer, m_culling_target->m_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.drawIndexedIndirect(m_culling_target->m_draw_indirect.getInfo().buffer, m_culling_target->m_draw_indirect.getInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));
		}

		cmd.endRenderPass();

		// visible calc
		{
			{
				auto to_transfer = m_culling_target->m_descriptors.m_draw_cmd.makeMemoryBarrier();
				to_transfer.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
				to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.updateBuffer<uint32_t>(m_culling_target->m_descriptors.m_draw_cmd.getInfo().buffer, m_culling_target->m_descriptors.m_draw_cmd.getInfo().offset + offsetof(vk::DrawIndexedIndirectCommand, instanceCount), 0u);

				auto to_read = m_culling_target->m_descriptors.m_draw_cmd.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead| vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
			}

			{
				auto to_read = m_culling_target->m_descriptors.m_visible.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
 				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_VISIBLE].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE].get(), 0, m_culling_target->m_descriptor_set.get(), {});
			cmd.dispatch(1, 1, 1);

			{
				auto to_read = m_culling_target->m_descriptors.m_draw_cmd.makeMemoryBarrier();
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, to_read, {});
			}
		}

		begin_render_Info.setRenderPass(m_render_pass->getRenderPass());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()));
		begin_render_Info.setFramebuffer(m_render_pass->getFramebuffer(context->getGPUFrame()));
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);
		{
			// マジで書く
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_GRAPHICS_RENDER].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_occluder->m_descriptor_set.get(), {});
			cmd.bindVertexBuffers(0, m_occluder->m_vertex.getInfo().buffer, m_occluder->m_vertex.getInfo().offset);
			cmd.bindIndexBuffer(m_occluder->m_index.getInfo().buffer, m_occluder->m_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.drawIndexedIndirect(m_occluder->m_draw_indirect.getInfo().buffer, m_occluder->m_draw_indirect.getInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW].get(), 0, m_culling_target->m_descriptor_set.get(), {});
			cmd.bindVertexBuffers(0, m_culling_target->m_vertex.getInfo().buffer, m_culling_target->m_vertex.getInfo().offset);
			cmd.bindIndexBuffer(m_culling_target->m_index.getInfo().buffer, m_culling_target->m_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.drawIndexedIndirect(m_culling_target->m_draw_indirect.getInfo().buffer, m_culling_target->m_draw_indirect.getInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

		}
		cmd.endRenderPass();
		cmd.end();
		return cmd;

	}

	std::shared_ptr<ResourceVertex> m_culling_target;
	std::shared_ptr<ResourceVertex> m_occluder;
	std::shared_ptr<RenderPassModule> m_render_pass;

	vk::UniqueRenderPass m_occlusion_test_pass;
	vk::UniqueFramebuffer m_occlusion_test_framebuffer;
};

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\009_culling\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(500.f, 300.f, -700.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;
	camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(1500.f, 300.f, 1400.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(1200, 800);
	app::App app(app_desc);

	auto context = app.m_context;
	CullingTest culling(context);

	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			auto cmd = culling.draw(context);
			app.submit(std::vector<vk::CommandBuffer>{cmd});
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

