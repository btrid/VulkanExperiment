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
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct Flowmap
{
	enum
	{
		DSL_Flowmap,
		DSL_Num,
	};
	enum
	{
		DS_Flowmap,
		DS_Num,
	};

	enum
	{
		Pipeline_Update,
		Pipeline_Render,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_Flowmap,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	vk::UniqueRenderPass m_RP_debugvoxel;
	vk::UniqueFramebuffer m_FB_debugvoxel;


	struct FlowmapInfo
	{
		ivec2 reso;
	};

	btr::BufferMemoryEx<FlowmapInfo> u_info;
	btr::BufferMemoryEx<float> b_value;

	FlowmapInfo m_info;
	std::vector<vec4> m_drops;

	Flowmap(btr::Context& ctx)
	{
		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL[DSL_Flowmap] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		// descriptor set
		{
			m_info.reso = uvec2(1024, 1024);
			uint num = m_info.reso.x * m_info.reso.y;
			u_info = ctx.m_uniform_memory.allocateMemory<FlowmapInfo>(1);
			b_value = ctx.m_storage_memory.allocateMemory<float>(num);

			cmd.updateBuffer<FlowmapInfo>(u_info.getInfo().buffer, u_info.getInfo().offset, m_info);
			cmd.fillBuffer(b_value.getInfo().buffer, b_value.getInfo().offset, b_value.getInfo().range, 0);

			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DSL_Flowmap].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[DS_Flowmap] = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] =
			{
				u_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] =
			{
				b_value.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(m_DS[DS_Flowmap].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(1)
				.setDstSet(m_DS[DS_Flowmap].get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DSL_Flowmap].get(),
					RenderTarget::s_descriptor_set_layout.get(),

				};
 				vk::PushConstantRange ranges[] =
 				{
 					vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setSize(sizeof(vec4)*4+8)
 				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_PL[PipelineLayout_Flowmap] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Flowmap_Update.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Flowmap_Render.comp.spv", vk::ShaderStageFlagBits::eCompute},
			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			vk::ComputePipelineCreateInfo compute_pipeline_info[] =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[0])
				.setLayout(m_PL[PipelineLayout_Flowmap].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[1])
				.setLayout(m_PL[PipelineLayout_Flowmap].get()),
			};
			m_pipeline[Pipeline_Update] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_Render] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
		}

	}

	void AddDrop(vec4 drop)
	{
		m_drops.push_back(drop);

	}

	void execute_Update(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		{
			{
				vk::BufferMemoryBarrier barrier[] =
				{
					b_value.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Update].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_Flowmap].get(), 0, { m_DS[DSL_Flowmap].get() }, {});

			for (int i = m_drops.size(); i < 4; i++)
			{
				m_drops.push_back(vec4(0.f));
			}
			cmd.pushConstants<vec4>(m_PL[PipelineLayout_Flowmap].get(), vk::ShaderStageFlagBits::eCompute, 0, m_drops);
			cmd.pushConstants<int>(m_PL[PipelineLayout_Flowmap].get(), vk::ShaderStageFlagBits::eCompute, sizeof(vec4) * 4, m_drops.size());

			static int seed;
			cmd.pushConstants<int>(m_PL[PipelineLayout_Flowmap].get(), vk::ShaderStageFlagBits::eCompute, sizeof(vec4) * 4+4, seed++%333333);
			m_drops.clear();

			auto num = app::calcDipatchGroups(uvec3(m_info.reso, 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	void execute_Render(vk::CommandBuffer cmd, RenderTarget& rt)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		{

			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
			image_barrier[0].setImage(rt.m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
		}


		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Render].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_Flowmap].get(), 0, { m_DS[DSL_Flowmap].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_Flowmap].get(), 1, { rt.m_descriptor.get() }, {});

		auto num = app::calcDipatchGroups(uvec3(m_info.reso, 1), uvec3(8, 8, 1));
		cmd.dispatch(num.x, num.y, num.z);

	}
};
int main()
{

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-400.f, 1500.f, -1430.f);
	camera->getData().m_target = vec3(800.f, 0.f, 1000.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	Flowmap flowmap(*context);
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	}
	app.setup();


	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				{
					if (context->m_window->getInput().m_mouse.isOn(cMouse::BUTTON_RIGHT))
					{
						flowmap.AddDrop(vec4(context->m_window->getInput().m_mouse.xy, 100.f, 0));
					}
					if (context->m_window->getInput().m_mouse.isOn(cMouse::BUTTON_LEFT))
					{
						flowmap.AddDrop(vec4(context->m_window->getInput().m_mouse.xy, 100.f, 1));
					}
					flowmap.execute_Update(cmd);
					flowmap.execute_Render(cmd, *app.m_window->getFrontBuffer());
				}

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

