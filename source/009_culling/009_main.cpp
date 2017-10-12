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
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

struct CullingDescriptorSetModule
{
	CullingDescriptorSetModule(const std::shared_ptr<btr::Context>& context, std::shared_ptr<DescriptorSetLayoutModule>& occlusion_layout)
	{
		m_occlusion_descriptor_set = occlusion_layout->allocateDescriptorSet(context);


		{
			std::vector<vk::DescriptorBufferInfo> storages = {
				m_world.getBufferInfo(),
				m_visible.getBufferInfo(),
				m_draw_cmd.getBufferInfo(),
				m_map.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(storages.size())
				.setPBufferInfo(storages.data())
				.setDstBinding(0)
				.setDstSet(m_occlusion_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(write_desc, {});
		}

	}

	btr::BufferMemory m_map;
	btr::BufferMemory m_visible;
	btr::BufferMemory m_draw_cmd;
	btr::BufferMemory m_world;
private:
	vk::UniqueDescriptorSet m_occlusion_descriptor_set;
};

struct CullingTest
{
	struct ResourceVertex
	{
		btr::BufferMemory m_mesh_vertex;
		btr::BufferMemory m_mesh_index;
		btr::BufferMemory m_mesh_indirect;
		btr::BufferMemory m_world;
		btr::BufferMemory m_visible_index;
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

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_OCCLUSION,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_OCCLUSION,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
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

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::shared_ptr<DescriptorSetLayoutModule> m_occlusion_descriptor_set_layout;
	std::shared_ptr<CullingDescriptorSetModule> m_occlusion_descriptor_set;

	CullingTest(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			std::shared_ptr<ResourceVertex> resource = std::make_shared<ResourceVertex>();
			std::vector<vec3> v;
			std::vector<uvec3> i;
			std::tie(v, i) = Geometry::MakeSphere();
			{

				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(v);
				resource->m_mesh_vertex = context->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), v.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(resource->m_mesh_vertex.getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd->copyBuffer(staging.getBufferInfo().buffer, resource->m_mesh_vertex.getBufferInfo().buffer, copy);
			}

			{
				btr::BufferMemoryDescriptor desc;
				desc.size = vector_sizeof(i);
				resource->m_mesh_index = context->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				memcpy(staging.getMappedPtr(), i.data(), desc.size);

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(resource->m_mesh_index.getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd->copyBuffer(staging.getBufferInfo().buffer, resource->m_mesh_index.getBufferInfo().buffer, copy);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(vk::DrawIndexedIndirectCommand);
				resource->m_mesh_indirect = context->m_vertex_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				auto* indirect_cmd = staging.getMappedPtr<vk::DrawIndexedIndirectCommand>();
				indirect_cmd->indexCount = i.size() * 3;
				indirect_cmd->instanceCount = 1000;
				indirect_cmd->firstIndex = 0;
				indirect_cmd->vertexOffset = 0;
				indirect_cmd->firstInstance = 0;

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(resource->m_mesh_indirect.getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd->copyBuffer(staging.getBufferInfo().buffer, resource->m_mesh_indirect.getBufferInfo().buffer, copy);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(mat4) * 1000;
				auto buffer = context->m_storage_memory.allocateMemory(desc);
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				auto* world = staging.getMappedPtr<mat4>();
				for (int i = 0; i < 1000; i++)
				{
					world[i] = glm::translate(glm::ballRand(1000.f));
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getBufferInfo().offset);
				copy.setDstOffset(buffer.getBufferInfo().offset);
				copy.setSize(desc.size);
				cmd->copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy);
				resource->m_world = buffer;
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t) * 1000;
				auto buffer = context->m_storage_memory.allocateMemory(desc);

				resource->m_visible_index = buffer;
			}
			m_culling_target = resource;
		}

		{
			m_render_pass = std::make_shared<RenderBackbufferModule>(context);
		}
		{
			std::vector<vk::DescriptorSetLayoutBinding> binding =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(2),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(3),
			};
			m_occlusion_descriptor_set_layout = std::make_shared<DescriptorSetLayoutModule>(context, binding, 1);
			m_occlusion_descriptor_set = std::make_shared<CullingDescriptorSetModule>(context, m_occlusion_descriptor_set_layout);
			{
				uint32_t num = 1000;
				{
					btr::BufferMemoryDescriptor desc;
					desc.size = sizeof(uint32_t) * num;
					m_occlusion_descriptor_set->m_map = context->m_storage_memory.allocateMemory(desc);
				}
				{
					btr::BufferMemoryDescriptor desc;
					desc.size = sizeof(uint32_t) * num;
					m_occlusion_descriptor_set->m_visible = context->m_storage_memory.allocateMemory(desc);
				}
				{
					m_occlusion_descriptor_set->m_world = m_culling_target->m_world;
				}
				{
					m_occlusion_descriptor_set->m_draw_cmd = m_culling_target->m_mesh_indirect;
				}
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
					{ "CalcVisibleGeometry.frag.spv",vk::ShaderStageFlagBits::eFragment },
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
				std::vector<vk::DescriptorSetLayout> layouts = 
				{
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_OCCLUSION].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
				m_pipeline_layout[PIPELINE_LAYOUT_DRAW] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}

			// setup pipeline
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
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
				depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
				depth_stencil_info.setStencilTestEnable(VK_FALSE);

				std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
					vk::PipelineColorBlendAttachmentState()
					.setBlendEnable(VK_FALSE)
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
						.setRenderPass(m_render_pass->getRenderPass())
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&blend_info),
					};
					auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
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
						.setRenderPass(m_render_pass->getRenderPass())
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&blend_info),
					};
					auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
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
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&blend_info),
					};
					auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
					m_pipeline[PIPELINE_GRAPHICS_OCCULSION_TEST] = std::move(pipelines[0]);

				}

				{
					// viewport
					std::vector<vk::Viewport> viewport =
					{
						vk::Viewport(0.f, 0.f, 320.f, (float)size.height, 0.f, 1.f),
						vk::Viewport(320.f, 0.f, 320.f, (float)size.height, 0.f, 1.f),
					};
					std::vector<vk::Rect2D> scissor =
					{
						vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(320, size.height)),
						vk::Rect2D(vk::Offset2D(320, 0), vk::Extent2D(320, size.height)),
					};
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
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&blend_info),
					};
					auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
					m_pipeline[PIPELINE_GRAPHICS_RENDER] = std::move(pipelines[0]);

				}

			}

		}
	}
	vk::CommandBuffer draw(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
//		cmd->
	}

	std::shared_ptr<ResourceVertex> m_culling_target;
	std::shared_ptr<RenderPassModule> m_render_pass;

};

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\009_culling\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;
	camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);

	app::App app;
	app.setup(gpu);

	auto context = app.m_context;

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			app.submit(std::vector<vk::CommandBuffer>{});
		}
		app.postUpdate();
		printf("%6.4fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

