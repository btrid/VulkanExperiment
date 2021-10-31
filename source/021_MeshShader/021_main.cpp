#include <btrlib/Define.h>

#include <btrlib/Context.h>

#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/sCameraManager.h>
#include <applib/sAppImGui.h>
#include <applib/GraphicsResource.h>
#include <applib/DrawHelper.h>


#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct Renderer
{
	enum
	{
		DSL_Renderer,
		DSL_Num,
	};
	enum
	{
		DS_Renderer,
		DS_Num,
	};

	enum
	{
		Pipeline_Render,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_Render,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	vk::UniqueRenderPass m_renderpass;
	vk::UniqueFramebuffer m_framebuffer;

	Renderer(btr::Context& ctx, RenderTarget& rt)
	{
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
// 				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
// 				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_Render] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// graphics pipeline render
		{

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
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					// depth
					vk::AttachmentDescription()
					.setFormat(rt.m_depth_info.format)
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
				m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);
			}

			{
				vk::ImageView view[] =
				{
					rt.m_view,
					rt.m_depth_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_renderpass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(rt.m_info.extent.width);
				framebuffer_info.setHeight(rt.m_info.extent.height);
				framebuffer_info.setLayers(1);
				m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Render.mesh.spv", vk::ShaderStageFlagBits::eMeshNV},
				{"Render.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport[] =
			{
				vk::Viewport(0.f, 0.f, (float)rt.m_resolution.width, (float)rt.m_resolution.height, 0.f, 1.f),
			};
			vk::Rect2D scissor[] =
			{
				vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution),
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


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
			blend_info.setAttachmentCount(array_size(blend_state));
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_size(shaderStages))
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL[PipelineLayout_Render].get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline[Pipeline_Render] = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}
	}


	void execute_Render(vk::CommandBuffer cmd, RenderTarget& rt)
	{
		DebugLabel _label(cmd, __FUNCTION__);
		{
			vk::ImageMemoryBarrier image_barrier[1];
			image_barrier[0].setImage(rt.m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Render].get());
// 		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.drawMeshTasksNV(1, 0);
		cmd.endRenderPass();

	}
};

int main()
{

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-0.2f, 3.1f, 0.02f);
	camera->getData().m_target = vec3(-0.2f, 1.1f, -0.03f);
	camera->getData().m_up = vec3(0.f, 0.f, 1.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);


	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	Renderer renderer(*context, *render_target);
	DrawHelper draw_helper{ context };

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
					renderer.execute_Render(cmd, *render_target);
					sAppImGui::Order().Render(cmd);
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

