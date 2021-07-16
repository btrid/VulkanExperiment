#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/sCameraManager.h>
#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


struct Renderer
{
	// graphics pipeline debug
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS;

	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_PL;

	vk::UniqueRenderPass m_RP;
	vk::UniqueFramebuffer m_FB;

	Renderer(const std::shared_ptr<btr::Context>& ctx, const std::shared_ptr<RenderTarget>& rt)
	{
		// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		// descriptor set
		{


			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS = std::move(ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			auto a = ctx->m_uniform_memory.allocateMemory(0);
// 			vk::DescriptorBufferInfo uniforms[] =
// 			{
// 				a.getInfo(),
// 			};
// 
// 			vk::WriteDescriptorSet write[] =
// 			{
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
// 				.setDescriptorCount(array_length(uniforms))
// 				.setPBufferInfo(uniforms)
// 				.setDstBinding(0)
// 				.setDstSet(m_DS.get()),
// 			};
// 			ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL.get(),
				};
				vk::PushConstantRange ranges[] =
				{
					vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eFragment).setSize(4)
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_PL = ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL.get(),
 					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL = ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// レンダーパス
		{
			vk::AttachmentReference color_ref[] = {
				vk::AttachmentReference().setLayout(vk::ImageLayout::eColorAttachmentOptimal).setAttachment(0),
			};
			vk::AttachmentReference depth_ref;
			depth_ref.setAttachment(1);
			depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);
			subpass.setPDepthStencilAttachment(&depth_ref);

			vk::AttachmentDescription attach_desc[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(rt->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				vk::AttachmentDescription()
				.setFormat(rt->m_depth_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_desc));
			renderpass_info.setPAttachments(attach_desc);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_RP = ctx->m_device.createRenderPassUnique(renderpass_info);

			{
				vk::ImageView view[] = {
					rt->m_view,
					rt->m_depth_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_RP.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(rt->m_info.extent.width);
				framebuffer_info.setHeight(rt->m_info.extent.height);
				framebuffer_info.setLayers(1);

				m_FB = ctx->m_device.createFramebufferUnique(framebuffer_info);
			}
		}
		struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
		{
			{"Ortho_Render.vert.spv", vk::ShaderStageFlagBits::eVertex},
			{"Ortho_Render.frag.spv", vk::ShaderStageFlagBits::eFragment},

		};
		std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
		std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
		for (size_t i = 0; i < array_length(shader_param); i++)
		{
			shader[i] = loadShaderUnique(ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
			shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
		}

		// assembly
		vk::PipelineInputAssemblyStateCreateInfo assembly_info;
		assembly_info.setPrimitiveRestartEnable(VK_FALSE);
		assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

		// viewport
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt->m_resolution.width, (float)rt->m_resolution.height, 0.f, 1.f);
		vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt->m_resolution.width, rt->m_resolution.height));
		vk::PipelineViewportStateCreateInfo viewportInfo;
		viewportInfo.setViewportCount(1);
		viewportInfo.setPViewports(&viewport);
		viewportInfo.setScissorCount(1);
		viewportInfo.setPScissors(&scissor);


		vk::PipelineRasterizationStateCreateInfo rasterization_info;
		rasterization_info.setPolygonMode(vk::PolygonMode::eLine);
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
		blend_state.setBlendEnable(VK_FALSE);
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

		vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(shaderStages.size())
			.setPStages(shaderStages.data())
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info)
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(m_PL.get())
			.setRenderPass(m_RP.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info);
		m_pipeline = ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

	}

	void execute(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& rt)
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

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 0, { m_DS.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 1, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 2, { rt->m_descriptor.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_RP.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt->m_info.extent.width, rt->m_info.extent.height)));
		begin_render_Info.setFramebuffer(m_FB.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.draw(36, 1, 0, 0);

		cmd.endRenderPass();

	}

};

int main()
{
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, -50.f);
	camera->getData().m_target = glm::vec3(0.001f, 0.001f, 0.001f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;
	camera->control(cInput(), 0.f);
	{
		auto R = transpose(mat3(camera->getData().m_view));
		vec3 s = R[0];
		vec3 u = R[1];
		vec3 f = R[2];

		vec3 e = camera->getPosition();
		vec3 p = vec3(10.f, 0, 0);
		vec3 V[8] =
		{
			vec3(-1.f, -1.f, -1.f),
			vec3( 1.f, -1.f, -1.f),
			vec3(-1.f,  1.f, -1.f),
			vec3( 1.f,  1.f, -1.f),
			vec3(-1.f, -1.f,  1.f),
			vec3( 1.f, -1.f,  1.f),
			vec3(-1.f,  1.f,  1.f),
			vec3( 1.f,  1.f,  1.f),
		};


		for (int i = 0; i < 8; i++)
		{
			auto v = V[i]*10.f+p;
			printf("%5.1f,%5.1f,%5.1f\n", v.x, v.y, v.z);
			vec3 dir = p-e;

			float sd = dot(dir, s);
			float ud = dot(dir, u);
			float fd = dot(dir, f);

			mat3 h = mat3(
// 				vec3(1, 0, abs(sd) <= 0.001 ? 0 : fd/sd),
// 				vec3(0, 1, 0/*abs(ud) <= 0.001 ? 0 : fd / ud*/),
// 				vec3(0, 0, 1)
 				vec3(1, 0, 0),
 				vec3(0, 1, 0),
 				vec3(sd / fd, 0, 1)
			);

			v = h*(dir) + e;
			printf("%8.4f,%8.4f,%8.4f\n", v.x, v.y, v.z);
			printf("\n");
		}

		int a = 0;
	}

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	app.setup();

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_render_target(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	Renderer renderer(context, app.m_window->getFrontBuffer());
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_clear,
				cmd_render,
				cmd_present,
				cmd_max,
			};
			std::vector<vk::CommandBuffer> cmds(cmd_max);
			cmds[cmd_clear] = clear_render_target.execute();
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				renderer.execute(cmd, render_target);
				cmd.end();
				cmds[cmd_render] = cmd;
			}
			cmds[cmd_present] = present_pipeline.execute();
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

