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
#include <btrlib/Singleton.h>
#include <btrlib/cWindow.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "imgui.lib")

struct Sky 
{
	enum Shader
	{
		Shader_Sky_CS,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Sky,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Sky_CS,
		Pipeline_Num,
	};

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	Sky(const std::shared_ptr<btr::Context>& context)
	{
		// descriptor layout
// 		{
// 			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
// 			vk::DescriptorSetLayoutBinding binding[] =
// 			{
// 				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
// 			};
// 			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
// 			desc_layout_info.setBindingCount(array_length(binding));
// 			desc_layout_info.setPBindings(binding);
// 			m_descriptor_set_layout = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
// 		}
// 		// descriptor set
// 		{
// 			vk::DescriptorSetLayout layouts[] = {
// 				m_descriptor_set_layout.get(),
// 			};
// 			vk::DescriptorSetAllocateInfo desc_info;
// 			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
// 			desc_info.setDescriptorSetCount(array_length(layouts));
// 			desc_info.setPSetLayouts(layouts);
// 			m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
// 
// 			vk::DescriptorBufferInfo uniforms[] = {
// 				u_radiosity_info.getInfo(),
// 				u_circle_mesh_count.getInfo(),
// 				u_circle_mesh_vertex.getInfo(),
// 			};
// 			vk::DescriptorBufferInfo storages[] = {
// 				b_segment_counter.getInfo(),
// 				b_segment.getInfo(),
// 				b_radiance.getInfo(),
// 				b_edge.getInfo(),
// 				b_albedo.getInfo(),
// 				v_emissive.getInfo(),
// 				v_emissive_draw_command.getInfo(),
// 				v_emissive_draw_count.getInfo(),
// 			};
// 
// 			vk::WriteDescriptorSet write[] =
// 			{
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
// 				.setDescriptorCount(array_length(uniforms))
// 				.setPBufferInfo(uniforms)
// 				.setDstBinding(0)
// 				.setDstSet(m_descriptor_set.get()),
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 				.setDescriptorCount(array_length(storages))
// 				.setPBufferInfo(storages)
// 				.setDstBinding(3)
// 				.setDstSet(m_descriptor_set.get()),
// 			};
// 			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
// 
// 			std::array<vk::WriteDescriptorSet, Frame_Num> write_desc;
// 			std::array<vk::DescriptorImageInfo, Frame_Num> image_info;
// 			for (int i = 0; i < Frame_Num; i++)
// 			{
// 				image_info[i] = vk::DescriptorImageInfo(m_image_sampler.get(), m_image_view[i].get(), vk::ImageLayout::eShaderReadOnlyOptimal);
// 
// 				write_desc[i] =
// 					vk::WriteDescriptorSet()
// 					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
// 					.setDescriptorCount(1)
// 					.setPImageInfo(&image_info[i])
// 					.setDstBinding(11)
// 					.setDstArrayElement(i)
// 					.setDstSet(m_descriptor_set.get());
// 			}
// 			context->m_device.updateDescriptorSets(array_length(write_desc), write_desc.data(), 0, nullptr);
// 		}

		// shader
		{
			const char* name[] =
			{
				"Sky.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					RenderTarget::s_descriptor_set_layout.get(),
//					m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_pipeline_layout[PipelineLayout_Sky] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}
		}

		// compute pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 6> shader_info;
			shader_info[0].setModule(m_shader[Shader_Sky_CS].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky].get()),
			};
			auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_Sky_CS] = std::move(compute_pipeline[0]);
		}
	}

	void execute(vk::CommandBuffer& cmd)
	{

	}
};
int main()
{
	btr::setResourceAppPath("..\\..\\resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, -500.f, 800.f);
	camera->getData().m_target = glm::vec3(0.f, -100.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);
	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());
	Sky sky(context);
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_model_update,
				cmd_model_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
			}

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			app.submit(std::move(cmds));
		}

		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

