#pragma once

#include <020_RT/ModelResource.h>

struct ModelRenderCmd
{
	VkDrawMeshTasksIndirectCommandNV task;
	uint32_t InstanceIndex;
	uint32_t _p;
};

struct ModelRenderer
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

	Context* m_ctx;
	ModelRenderer(Context& ctx, RenderTarget& rt)
	{
		m_ctx = &ctx;
		//		auto cmd = ctx.m_ctx->m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eMeshNV| vk::ShaderStageFlagBits::eTaskNV;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo dsl_CI;
			dsl_CI.setBindingCount(array_length(binding));
			dsl_CI.setPBindings(binding);
			dsl_CI.setFlags(vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool);
			vk::DescriptorSetLayoutBindingFlagsCreateInfo dslflagCI;
			std::vector<vk::DescriptorBindingFlags> flags = { vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound, vk::DescriptorBindingFlagBits::eUpdateAfterBind | vk::DescriptorBindingFlagBits::ePartiallyBound };
			dslflagCI.setBindingFlags(flags);
			dsl_CI.setPNext(&dslflagCI);
			m_DSL[DSL_Renderer] = ctx.m_ctx->m_device.createDescriptorSetLayoutUnique(dsl_CI);
		}

		// descriptor set
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DSL_Renderer].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[DS_Renderer] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					ctx.m_DSL[Context::DSL_Scene].get(),
					ctx.m_model_resource.m_DSL.get(),
					m_DSL[DSL_Renderer].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_Render] = ctx.m_ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
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
				m_renderpass = ctx.m_ctx->m_device.createRenderPassUnique(renderpass_info);
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
				m_framebuffer = ctx.m_ctx->m_device.createFramebufferUnique(framebuffer_info);
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"ModelRender2.task.spv", vk::ShaderStageFlagBits::eTaskNV},
				{"ModelRender2.mesh.spv", vk::ShaderStageFlagBits::eMeshNV},
				{"ModelRender.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
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
			m_pipeline[Pipeline_Render] = ctx.m_ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}

	}

	void execute_Render(vk::CommandBuffer cmd, RenderTarget& rt, Model& model)
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
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 1, { m_ctx->m_DS_Scene.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 2, { m_ctx->m_model_resource.m_DS_ModelResource.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		auto& gltf_model = model.gltf_model;
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			const auto& gltf_mesh = gltf_model.meshes[mesh_index];

			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
			{
				const tinygltf::Primitive& primitive = gltf_mesh.primitives[i];
				{
					const tinygltf::Accessor& accessor = gltf_model.accessors[primitive.indices];
					const tinygltf::BufferView& bufferview = gltf_model.bufferViews[accessor.bufferView];


					static const uint32_t MESHLETS_PER_TASK = 32;
					uint32_t count = app::calcDipatchGroups(uvec3(model.m_MeshletGeometry[0].meshletDescriptors.size(), 1, 1), uvec3(MESHLETS_PER_TASK, 1, 1)).x;
					cmd.drawMeshTasksNV(count, 0);
				}
			}
		}

		cmd.endRenderPass();
	}

	void execute_Render(vk::CommandBuffer cmd, RenderTarget& rt, const vk::DescriptorBufferInfo& rendercmd, const vk::DescriptorBufferInfo& count, const vk::DescriptorBufferInfo& instance)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		vk::DescriptorBufferInfo storages[] =
		{
			rendercmd,
			instance,
		};
		vk::WriteDescriptorSet write[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(m_DS[DS_Renderer].get()),
		};
		m_ctx->m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

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
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 1, { m_ctx->m_DS_Scene.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 2, { m_ctx->m_model_resource.m_DS_ModelResource.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 3, { m_DS[DS_Renderer].get()}, {});


		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.drawMeshTasksIndirectCountNV(rendercmd.buffer, rendercmd.offset, count.buffer, count.offset, 1024, sizeof(ModelRenderCmd));
//		cmd.drawMeshTasksIndirectNV(rendercmd.buffer, rendercmd.offset, 1, sizeof(ModelRenderCmd));

		cmd.endRenderPass();
	}
};
