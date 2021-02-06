#pragma once


namespace csg
{
struct LDPoint
{
	float p;
	int32_t flags;
};
struct Renderer
{
	enum
	{
		DS_LayeredDepth,
		DS_Num,
	};
	enum
	{
		Pipeline_Boolean_Add,
		Pipeline_Boolean_Sub,
		Pipeline_MakeDepthMap,
		Pipeline_Rendering,
		Pipeline_Num,
	};

	enum
	{
		PipelineLayout_LayeredDepth,
		PipelineLayout_MakeDepthMap,
		PipelineLayout_Rendering,
		PipelineLayout_Num,
	};

	btr::BufferMemoryEx<ivec2> b_ldc_counter;
	btr::BufferMemoryEx<LDPoint> b_ldc_point;
	btr::BufferMemoryEx<int32_t> b_ldc_point_link_head;
	btr::BufferMemoryEx<int32_t> b_ldc_point_link_next;
	btr::BufferMemoryEx<int32_t> b_ldc_point_memory_block;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_render_framebuffer;
	vk::UniqueRenderPass m_make_depth_pass;
	vk::UniqueFramebuffer m_make_depth_framebuffer;

	std::array<vk::UniqueDescriptorSetLayout, DS_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pl;

	struct Instance
	{
		::Model* model;
		ModelInstance instance;
	};
	std::vector<Instance> m_instance;
	Renderer(btr::Context& ctx, DCContext& dc_ctx, const RenderTarget& rt)
	{
		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL[DS_LayeredDepth] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);

		}

		// descriptor set
		{
			b_ldc_counter = ctx.m_storage_memory.allocateMemory<ivec2>(1);
			b_ldc_point_link_head = ctx.m_storage_memory.allocateMemory<int>(1024 * 1024);
			b_ldc_point_link_next = ctx.m_storage_memory.allocateMemory<int>(1024 * 1024 * 64);
			b_ldc_point = ctx.m_storage_memory.allocateMemory<LDPoint>(1024 * 1024 * 64);
			b_ldc_point_memory_block = ctx.m_storage_memory.allocateMemory<int32_t>(1);

			{
				cmd.fillBuffer(b_ldc_counter.getInfo().buffer, b_ldc_counter.getInfo().offset, b_ldc_counter.getInfo().range, 0);
				cmd.fillBuffer(b_ldc_point_link_head.getInfo().buffer, b_ldc_point_link_head.getInfo().offset, b_ldc_point_link_head.getInfo().range, -1);
				cmd.fillBuffer(b_ldc_point_memory_block.getInfo().buffer, b_ldc_point_memory_block.getInfo().offset, b_ldc_point_memory_block.getInfo().range, -1);
				vk::BufferMemoryBarrier barrier[] =
				{
					b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_ldc_point_memory_block.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
			}

			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DS_LayeredDepth].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[DS_LayeredDepth] = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] =
			{
				b_ldc_counter.getInfo(),
				b_ldc_point_link_head.getInfo(),
				b_ldc_point_link_next.getInfo(),
				b_ldc_point.getInfo(),
				b_ldc_point_memory_block.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_DS[DS_LayeredDepth].get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}



		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DS_LayeredDepth].get(),
					dc_ctx.m_DSL[DCContext::DSL_Model].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pl[PipelineLayout_LayeredDepth] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DS_LayeredDepth].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pl[PipelineLayout_MakeDepthMap] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}

			{
				vk::DescriptorSetLayout layouts[] =
				{
					dc_ctx.m_DSL[DCContext::DSL_Model].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pl[PipelineLayout_Rendering] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"LayeredDepth_BooleanAdd.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"LayeredDepth_BooleanSub.comp.spv", vk::ShaderStageFlagBits::eCompute},
			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[0])
				.setLayout(m_pl[PipelineLayout_LayeredDepth].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[1])
				.setLayout(m_pl[PipelineLayout_LayeredDepth].get()),
			};
			m_pipeline[Pipeline_Boolean_Add] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_Boolean_Sub] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
		}
		{
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
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					vk::AttachmentDescription()
					.setFormat(rt.m_depth_info.format)
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

				m_render_pass = ctx.m_device.createRenderPassUnique(renderpass_info);

				{
					vk::ImageView view[] = {
						rt.m_view,
						rt.m_depth_view,
					};
					vk::FramebufferCreateInfo framebuffer_info;
					framebuffer_info.setRenderPass(m_render_pass.get());
					framebuffer_info.setAttachmentCount(array_length(view));
					framebuffer_info.setPAttachments(view);
					framebuffer_info.setWidth(rt.m_info.extent.width);
					framebuffer_info.setHeight(rt.m_info.extent.height);
					framebuffer_info.setLayers(1);

					m_render_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);
				}
			}

			// graphics pipeline render
			{
				struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
				{
					{"CSG_RenderModel.vert.spv", vk::ShaderStageFlagBits::eVertex},
					{"CSG_RenderModel.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
				vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt.m_resolution.width, (float)rt.m_resolution.height, 0.f, 1.f);
				vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_resolution.width, rt.m_resolution.height));
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
				depth_stencil_info.setDepthWriteEnable(VK_FALSE);
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eEqual);
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
					.setLayout(m_pl[PipelineLayout_Rendering].get())
					.setRenderPass(m_render_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info);
				m_pipeline[Pipeline_Rendering] = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

			}

			// graphics pipeline render
			{
				// render pass
				{
					vk::AttachmentReference depth_ref;
					depth_ref.setAttachment(1);
					depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

					// sub pass
					vk::SubpassDescription subpass;
					subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
					subpass.setInputAttachmentCount(0);
					subpass.setPInputAttachments(nullptr);
					subpass.setPDepthStencilAttachment(&depth_ref);

					vk::AttachmentDescription attach_desc[] =
					{
						vk::AttachmentDescription()
						.setFormat(rt.m_depth_info.format)
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

					m_make_depth_pass = ctx.m_device.createRenderPassUnique(renderpass_info);

					{
						vk::ImageView view[] = {
							rt.m_depth_view,
						};
						vk::FramebufferCreateInfo framebuffer_info;
						framebuffer_info.setRenderPass(m_render_pass.get());
						framebuffer_info.setAttachmentCount(array_length(view));
						framebuffer_info.setPAttachments(view);
						framebuffer_info.setWidth(rt.m_info.extent.width);
						framebuffer_info.setHeight(rt.m_info.extent.height);
						framebuffer_info.setLayers(1);

						m_make_depth_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);
					}
				}

				struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
				{
					{"LayeredDepth_MakeDepthMap.vert.spv", vk::ShaderStageFlagBits::eVertex},
					{"LayeredDepth_MakeDepthMap.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
				vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt.m_resolution.width, (float)rt.m_resolution.height, 0.f, 1.f);
				vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_resolution.width, rt.m_resolution.height));
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
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
				depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
				depth_stencil_info.setStencilTestEnable(VK_FALSE);


				vk::PipelineColorBlendAttachmentState blend_state;
				blend_state.setBlendEnable(VK_FALSE);
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
					.setLayout(m_pl[PipelineLayout_MakeDepthMap].get())
					.setRenderPass(m_make_depth_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info);
				m_pipeline[Pipeline_MakeDepthMap] = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

			}
		}
	}
	void Request(::Model& model, const ModelInstance& instance)
	{
		m_instance.emplace_back(&model, instance);
	}
	void ExecuteRendering(vk::CommandBuffer cmd, RenderTarget& rt)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		_label.insert("Clear");
		{
			{
				vk::BufferMemoryBarrier barrier[] =
				{
					b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_size(barrier), barrier }, {});

			}
			cmd.fillBuffer(b_ldc_counter.getInfo().buffer, b_ldc_counter.getInfo().offset, b_ldc_counter.getInfo().range, 0);
			cmd.fillBuffer(b_ldc_point_link_head.getInfo().buffer, b_ldc_point_link_head.getInfo().offset, b_ldc_point_link_head.getInfo().range, -1);


		}
		_label.insert("Make Layered Depth");
		{
			vk::BufferMemoryBarrier barrier[] =
			{
				b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Boolean_Add].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pl[PipelineLayout_LayeredDepth].get(), 0, m_DS[DS_LayeredDepth].get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pl[PipelineLayout_LayeredDepth].get(), 2, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
			for (auto& instance : m_instance)
			{
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pl[PipelineLayout_LayeredDepth].get(), 1, instance.model->m_DS_Model.get(), {});

				auto size = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(8, 8, 1));
				cmd.dispatch(size.x, size.y, size.z);
			}
		}

		_label.insert("Make DepthMap ");
		{
			std::array<vk::BufferMemoryBarrier, 1> barrier =
			{
				b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
// 			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
// 			image_barrier[0].setImage(rt.m_depth_image);
// 			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
//			image_barrier[0].setOldLayout(vk::ImageLayout:: eColorAttachmentOptimal);
//			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
// 			image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
// 			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

//			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, image_barrier);

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_info.extent.width, rt.m_info.extent.height)));
			begin_render_Info.setFramebuffer(m_render_framebuffer.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_MakeDepthMap].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl[PipelineLayout_MakeDepthMap].get(), 0, m_DS[DS_LayeredDepth].get(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl[PipelineLayout_MakeDepthMap].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
			cmd.draw(3, 1, 0, 0);

			cmd.endRenderPass();

		}


		_label.insert("Rendering");
		{
			{
				vk::ImageMemoryBarrier image_barrier[1];
				image_barrier[0].setImage(rt.m_image);
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
				image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
// 				image_barrier[1].setImage(rt.m_depth_image);
// 				image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
// 				image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
// 				image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
// 				image_barrier[1].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
// 				image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
					{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
			}

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_info.extent.width, rt.m_info.extent.height)));
			begin_render_Info.setFramebuffer(m_render_framebuffer.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl[PipelineLayout_Rendering].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
			for (auto& instance : m_instance)
			{
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl[PipelineLayout_Rendering].get(), 0, instance.model->m_DS_Model.get(), {});

				vec3 dir = normalize(instance.instance.dir.xyz());
				auto rot = quat(vec3(0.f, 0.f, 1.f), dir);

				mat4 world = translate(instance.instance.pos.xyz()) * mat4(rot);
				cmd.pushConstants<mat4>(m_pl[PipelineLayout_Rendering].get(), vk::ShaderStageFlagBits::eVertex, 0, world);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Rendering].get());
				cmd.drawIndirect(instance.model->b_draw_cmd.getInfo().buffer, instance.model->b_draw_cmd.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

			}



			cmd.endRenderPass();
		}

		m_instance.clear();
	}

};

int main()
{
	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(50.f, 50.f, -500.f);
	camera->getData().m_target = vec3(256.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	auto dc_ctx = std::make_shared<DCContext>(context);

	auto model_box = Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Box.dae", { 100.f });
	auto model = Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Duck.dae", { 2.5f });

	csg::Renderer renderer(*context, *dc_ctx, *app.m_window->getFrontBuffer());

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	ModelInstance instance_list[30];
	for (auto& i : instance_list) { i = { vec4(glm::linearRand(vec3(0.f), vec3(500.f)), 0.f), vec4(glm::ballRand(1.f), 100.f) }; }

	app.setup();

	struct DynamicInstance
	{
		float time;
		vec3 rot[2];
		vec3 pos[2];

		ModelInstance get()
		{
			time += 0.002f;
			if (time >= 1.f)
			{
				time = 0.f;
				rot[0] = rot[1]; rot[1] = glm::ballRand(1.f);
				pos[0] = pos[1]; pos[1] = glm::linearRand(vec3(0.f), vec3(500.f));
			}
			return { vec4(mix(pos[0], pos[1], time), 100.f), vec4(mix(rot[0], rot[1], time), 0.f) };
		}
	};
	DynamicInstance dynamic_instance{ 1.f, {vec4(0.f, 1.f, 1.f, 0.f), vec4(0.f, 1.f, 1.f, 0.f) }, {vec4(200.f), vec4(200.f)} };


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
					for (auto& i : instance_list) renderer.Request(*model, i);

					renderer.ExecuteRendering(cmd, *app.m_window->getFrontBuffer());
//					renderer.ExecuteTestRender(cmd, *dc_ctx, *dc_model, *app.m_window->getFrontBuffer());
				}

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
			context->m_device.waitIdle();

		}
		app.postUpdate();
		printf("%-6.3fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}
}
