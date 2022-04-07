#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <unordered_map>
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
#include <btrlib/AllocatedMemory.h>
//#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#include <003_particle/emps.h>

#include <applib/sAppImGui.h>

//template<typename N, typename T>
struct RenderNode
{
//	constexpr size_t name() { std::hash<N>; }
//	constexpr size_t hash() { std::hash<std::string>(N); }
};
struct RenderGraph
{
	std::unordered_map<std::string, RenderNode> m_graph;

	template<typename N, typename T, typename... Args>
	constexpr T Add(Args... args)
	{
		return m_graph[N].emplace(args);
	}
};

void gui(FluidData& cFluid)
{
	app::g_app_instance->m_window->getImgui()->pushImguiCmd([&]()
		{
			ImGui::SetNextWindowSize(ImVec2(200.f, 40.f), ImGuiCond_Once);
			static bool is_open;
			if (ImGui::Begin("FluidConfig", &is_open, ImGuiWindowFlags_NoSavedSettings))
			{
				ImGui::SliderFloat("viscosity", &cFluid.viscosity, 0.f, 30.f);
			}

			ImGui::End();
		});

}


struct FluidRenderer
{
	enum Pipelines
	{
		Pipeline_Rendering_Fluid,
		Pipeline_Rendering_Wall,
		Pipeline_Max,
	};
	std::array<vk::UniquePipeline, Pipeline_Max> m_pipeline;
	vk::UniquePipelineLayout m_PL;


	FluidRenderer(btr::Context& ctx, FluidContext& cFluid, const std::shared_ptr<RenderTarget>& rt)
	{
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					cFluid.m_DSL.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"P_Render.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"P_Render.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt->m_resolution.width, (float)rt->m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt->m_resolution.width, rt->m_resolution.height));
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount(1);
			viewportInfo.setPScissors(&scissor);

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);
			//		rasterization_info.setRasterizerDiscardEnable(VK_TRUE);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);


			vk::PipelineColorBlendAttachmentState blend_state;
			blend_state.setBlendEnable(VK_TRUE);
			blend_state.setColorBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcColorBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstColorBlendFactor(vk::BlendFactor::eZero);
			blend_state.setAlphaBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
			blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA);

			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(1);
			blend_info.setPAttachments(&blend_state);

			auto bindings = {
				vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(vec3)),
				vk::VertexInputBindingDescription().setBinding(1).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(int32_t)),
			};
			auto attributes = {
				vk::VertexInputAttributeDescription().setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
				vk::VertexInputAttributeDescription().setBinding(1).setLocation(1).setFormat(vk::Format::eR32Sint).setOffset(0),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptions(bindings);
			vertex_input_info.setVertexAttributeDescriptions(attributes);

			vk::PipelineDynamicStateCreateInfo DynamicStateCI;
			auto ds = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
			DynamicStateCI.setDynamicStates(ds);

			vk::GraphicsPipelineCreateInfo graphics_pipeline_CI =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				//			.setPDynamicState(&DynamicStateCI)
				.setLayout(m_PL.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);

			auto color_formats = { rt->m_info.format };
			vk::PipelineRenderingCreateInfoKHR pipeline_rendering_CI = vk::PipelineRenderingCreateInfoKHR().setColorAttachmentFormats(color_formats).setDepthAttachmentFormat(rt->m_depth_info.format);

			graphics_pipeline_CI.setPNext(&pipeline_rendering_CI);
			m_pipeline[Pipeline_Rendering_Fluid] = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_CI).value;
		}
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Wall_Render.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"Wall_Render.geom.spv", vk::ShaderStageFlagBits::eGeometry},
				{"Wall_Render.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt->m_resolution.width, (float)rt->m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt->m_resolution.width, rt->m_resolution.height));
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount(1);
			viewportInfo.setPScissors(&scissor);

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


			vk::PipelineColorBlendAttachmentState blend_state;
			blend_state.setBlendEnable(VK_TRUE);
			blend_state.setColorBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcColorBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstColorBlendFactor(vk::BlendFactor::eZero);
			blend_state.setAlphaBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
			blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA);

			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(1);
			blend_info.setPAttachments(&blend_state);

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineDynamicStateCreateInfo DynamicStateCI;
			auto ds = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
			DynamicStateCI.setDynamicStates(ds);

			vk::GraphicsPipelineCreateInfo graphics_pipeline_CI =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				//			.setPDynamicState(&DynamicStateCI)
				.setLayout(m_PL.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);

			auto color_formats = { rt->m_info.format };
			vk::PipelineRenderingCreateInfoKHR pipeline_rendering_CI = vk::PipelineRenderingCreateInfoKHR().setColorAttachmentFormats(color_formats).setDepthAttachmentFormat(rt->m_depth_info.format);

			graphics_pipeline_CI.setPNext(&pipeline_rendering_CI);
			m_pipeline[Pipeline_Rendering_Wall] = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_CI).value;

		}
	}


	void execute(vk::CommandBuffer cmd, btr::Context& ctx, const std::shared_ptr<RenderTarget>& rt, FluidData& dFluid)
	{
		{
			vk::ImageMemoryBarrier image_barrier[1];
			image_barrier[0].setImage(rt->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
		}

		vk::RenderingAttachmentInfoKHR color[] = {
			vk::RenderingAttachmentInfoKHR().setImageView(rt->m_view).setStoreOp(vk::AttachmentStoreOp::eStore).setLoadOp(vk::AttachmentLoadOp::eLoad).setImageLayout(vk::ImageLayout::eColorAttachmentOptimal),
		};
		vk::RenderingAttachmentInfoKHR depth[] = {
			vk::RenderingAttachmentInfoKHR().setImageView(rt->m_depth_view).setStoreOp(vk::AttachmentStoreOp::eStore).setLoadOp(vk::AttachmentLoadOp::eLoad).setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
		};

		vk::RenderingInfoKHR rendering_info;
		rendering_info.colorAttachmentCount = array_size(color);
		rendering_info.pColorAttachments = color;
		rendering_info.pDepthAttachment = depth;
		rendering_info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt->m_info.extent.width, rt->m_info.extent.height)));
		rendering_info.setLayerCount(1);

		cmd.beginRenderingKHR(rendering_info);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 1, { dFluid.m_DS.get() }, {});
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Rendering_Fluid].get());

			auto v = ctx.m_staging_memory.allocateMemory<vec3>(dFluid.PNum);
			auto t = ctx.m_staging_memory.allocateMemory<int32_t>(dFluid.PNum);
			for (int i = 0; i < dFluid.PNum; i++) { *v.getMappedPtr(i) = dFluid.Pos[i]; }
			for (int i = 0; i < dFluid.PNum; i++) { *t.getMappedPtr(i) = dFluid.PType[i]; }

			cmd.bindVertexBuffers(0, { v.getInfo().buffer, t.getInfo().buffer }, { v.getInfo().offset, t.getInfo().offset });
			cmd.draw(dFluid.PNum, 1, 0, 0);
		}

		{
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Rendering_Wall].get());
			cmd.draw(dFluid.m_constant.GridCellTotal, 1, 0, 0);
		}


		cmd.endRenderingKHR();
	}
};
struct Particle
{
	Particle(btr::Context& ctx)
	{

	}
	void execute(vk::CommandBuffer cmd, btr::Context& ctx)
	{
		app::g_app_instance->m_window->getImgui()->pushImguiCmd([this]()
			{
				ImGui::SetNextWindowSize(ImVec2(400.f, 300.f), ImGuiCond_Once);
				static bool is_open;
				ImGui::Begin("RenderConfig", &is_open, ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_HorizontalScrollbar);
				if (ImGui::BeginChild("editor", ImVec2(-1, -1), false, ImGuiWindowFlags_HorizontalScrollbar)) {
					static char text[1024 * 16] =
						"/*\n"
						" The Pentium F00F bug, shorthand for F0 0F C7 C8,\n"
						" the hexadecimal encoding of one offending instruction,\n"
						" more formally, the invalid operand with locked CMPXCHG8B\n"
						" instruction bug, is a design flaw in the majority of\n"
						" Intel Pentium, Pentium MMX, and Pentium OverDrive\n"
						" processors (all in the P5 microarchitecture).\n"
						"*/\n\n"
						"label:\n"
						"\tlock cmpxchg8b eax\n";
					ImGui::InputTextMultiline("##source", text, std::size(text), ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput);
				}

				ImGui::EndChild();
				ImGui::End();
			});

	}
	void draw(vk::CommandBuffer cmd)
	{

	}

};

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\003_particle\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(-0.3f, 0.1f, -0.3f);
	camera->getData().m_target = glm::vec3(1.f, -0.1f, 1.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_render_target(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), context->m_window->getSwapchain());

	FluidContext cFluid(*context);
	FluidData dFluid;
	dFluid.triangle = Triangle(vec3(0.f, 0.1f, 0.f), vec3(0.2f, 0.1f, 0.f), vec3(0.f, 0.1f, 0.2f));


	init(dFluid);
	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);
	dFluid.u_constant = context->m_uniform_memory.allocateMemory<FluidData::Constant>(setup_cmd, context->m_staging_memory, dFluid.m_constant);
	dFluid.b_WallEnable = context->m_storage_memory.allocateMemory<int32_t>(dFluid.wallenable.size());
	setup_cmd.updateBuffer<int32_t>(dFluid.b_WallEnable.getInfo().buffer, dFluid.b_WallEnable.getInfo().offset, dFluid.wallenable);
	// descriptor set
	{
		vk::DescriptorSetLayout layouts[] =
		{
			cFluid.m_DSL.get(),
		};
		vk::DescriptorSetAllocateInfo desc_info;
		desc_info.setDescriptorPool(context->m_descriptor_pool.get());
		desc_info.setDescriptorSetCount(array_length(layouts));
		desc_info.setPSetLayouts(layouts);
		dFluid.m_DS = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
		{
			auto uniforms =
			{
				dFluid.u_constant.getInfo()
			};
			auto storages =
			{
				dFluid.b_WallEnable.getInfo()
			};
			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet().setDstSet(*dFluid.m_DS).setDstBinding(0).setDescriptorType(vk::DescriptorType::eUniformBuffer).setBufferInfo(uniforms),
				vk::WriteDescriptorSet().setDstSet(*dFluid.m_DS).setDstBinding(10).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(storages)
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}


	}


	FluidRenderer rFluid(*context, cFluid, app.m_window->getFrontBuffer());
	Particle particle(*context);
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum cmds
			{
				cmd_clear,
				cmd_particle,
				cmd_present,
				cmd_num,
			};
			std::vector<vk::CommandBuffer> render_cmds(cmd_num);
			{
				render_cmds[cmd_clear] = clear_render_target.execute();
				render_cmds[cmd_present] = present_pipeline.execute();
			}

			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

				gui(dFluid);
				run(dFluid);
				rFluid.execute(cmd, *context, app.m_window->getFrontBuffer(), dFluid);

				sAppImGui::Order().Render(cmd);
				cmd.end();
				render_cmds[cmd_particle] = cmd;
			}


			app.submit(std::move(render_cmds));
		}

		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

