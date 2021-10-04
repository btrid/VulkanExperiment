#pragma once


struct Model
{
	struct Info
	{
		vec4 m_aabb_min;
		vec4 m_aabb_max;
		uint m_vertex_num;
		uint m_primitive_num;
		uvec3 m_voxel_reso;
	};

	struct Mesh
	{
		std::vector<vk::VertexInputBindingDescription> vib_disc;
		std::vector<vk::VertexInputAttributeDescription> vid_disc;
	};

	struct Material
	{
		vec4 m_basecolor_factor;
		float m_metallic_factor;
		float m_roughness_factor;
		float _p1;
		float _p2;
		vec3  m_emissive_factor;
		float _p11;
	};
	std::vector<Mesh> m_meshes;
	std::vector<btr::BufferMemory> b_vertex;
	std::vector<ResourceTexture> t_image;
	std::vector<btr::BufferMemoryEx<Material>> b_material;
	std::vector<vk::UniqueDescriptorSet> m_DS_material;

	tinygltf::Model gltf_model;

	vk::UniqueDescriptorSet m_DS_Model;

	Info m_info;

	static std::shared_ptr<Model> LoadModel(Context& ctx, vk::CommandBuffer cmd, const std::string& filename)
	{
		tinygltf::Model gltf_model;
		{
			tinygltf::TinyGLTF loader;
			//			loader.SetImageLoader()
			std::string err;
			std::string warn;

			//			loader.
			bool res = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, filename);
			if (!warn.empty()) { std::cout << "WARN: " << warn << std::endl; }
			if (!err.empty()) { std::cout << "ERR: " << err << std::endl; }

			if (!res) std::cout << "Failed to load glTF: " << filename << std::endl;
			else std::cout << "Loaded glTF: " << filename << std::endl;
		}

		auto model = std::make_shared<Model>();

		// buffer
		{
			model->b_vertex.resize(gltf_model.buffers.size());
			std::vector<btr::BufferMemory> staging(gltf_model.buffers.size());
			for (int i = 0; i < gltf_model.buffers.size(); i++)
			{
				model->b_vertex[i] = ctx.m_ctx->m_vertex_memory.allocateMemory(gltf_model.buffers[i].data.size());
				staging[i] = ctx.m_ctx->m_staging_memory.allocateMemory(gltf_model.buffers[i].data.size());
				memcpy_s(staging[i].getMappedPtr(), gltf_model.buffers[i].data.size(), gltf_model.buffers[i].data.data(), gltf_model.buffers[i].data.size());

				vk::BufferCopy copy = vk::BufferCopy().setSize(gltf_model.buffers[i].data.size()).setSrcOffset(staging[i].getInfo().offset).setDstOffset(model->b_vertex[i].getInfo().offset);
				cmd.copyBuffer(staging[i].getInfo().buffer, model->b_vertex[i].getInfo().buffer, copy);
			}
		}

		// image 
		{
			model->t_image.resize(gltf_model.images.size());
			for (size_t i = 0; i < gltf_model.images.size(); i++)
			{
				tinygltf::Texture& tex = gltf_model.textures[i];
				tinygltf::Image& image = gltf_model.images[i];

				vk::ImageCreateInfo image_info;
				image_info.imageType = vk::ImageType::e2D;
				image_info.format = vk::Format::eR8G8B8A8Unorm;//GLtoVK::toFormat(image.pixel_type, image.component, image.bits);
				image_info.mipLevels = 1;
				image_info.arrayLayers = 1;
				image_info.samples = vk::SampleCountFlagBits::e1;
				image_info.tiling = vk::ImageTiling::eLinear;
				image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
				image_info.sharingMode = vk::SharingMode::eExclusive;
				image_info.initialLayout = vk::ImageLayout::eUndefined;
				image_info.extent = vk::Extent3D(image.width, image.height, 1);

				model->t_image[i].make(*ctx.m_ctx, cmd, image.uri, image_info, image.image);
			}
		}
		// material
		{
			model->b_material.resize(gltf_model.materials.size());
			model->m_DS_material.resize(gltf_model.materials.size());
			vk::DescriptorSetLayout layouts[] =
			{
				ctx.m_DSL[Context::DSL_Model_Material].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);

			for (size_t i = 0; i < gltf_model.materials.size(); i++)
			{
				auto gltf_material = gltf_model.materials[i];


				Material material;
				material.m_basecolor_factor.x = gltf_material.pbrMetallicRoughness.baseColorFactor[0];
				material.m_basecolor_factor.y = gltf_material.pbrMetallicRoughness.baseColorFactor[1];
				material.m_basecolor_factor.z = gltf_material.pbrMetallicRoughness.baseColorFactor[2];
				material.m_basecolor_factor.w = gltf_material.pbrMetallicRoughness.baseColorFactor[3];
				material.m_metallic_factor = gltf_material.pbrMetallicRoughness.metallicFactor;
				material.m_roughness_factor = gltf_material.pbrMetallicRoughness.roughnessFactor;
				material.m_emissive_factor.x = gltf_material.emissiveFactor[0];
				material.m_emissive_factor.y = gltf_material.emissiveFactor[1];
				material.m_emissive_factor.z = gltf_material.emissiveFactor[2];
				model->b_material[i] = ctx.m_ctx->m_uniform_memory.allocateMemory<Material>(1);
				cmd.updateBuffer<Material>(model->b_material[i].getInfo().buffer, model->b_material[i].getInfo().offset, material);

				// descriptor set
				{

					model->m_DS_material[i] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

					vk::DescriptorBufferInfo uniforms[] =
					{
						model->b_material[i].getInfo(),
					};
					std::array<vk::DescriptorImageInfo, 5> samplers;
					auto& t = sGraphicsResource::Order().getWhiteTexture();
					samplers.fill(vk::DescriptorImageInfo(t.m_sampler.get(), t.m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));

					if (gltf_material.pbrMetallicRoughness.baseColorTexture.index >= 0)
					{
						samplers[0].sampler = model->t_image[gltf_material.pbrMetallicRoughness.baseColorTexture.index].getSampler();
						samplers[0].imageView = model->t_image[gltf_material.pbrMetallicRoughness.baseColorTexture.index].getImageView();
						samplers[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}
					if (gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
					{
						samplers[1].sampler = model->t_image[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index].getSampler();
						samplers[1].imageView = model->t_image[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index].getImageView();
						samplers[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}
					if (gltf_material.normalTexture.index >= 0)
					{
						samplers[2].sampler = model->t_image[gltf_material.normalTexture.index].getSampler();
						samplers[2].imageView = model->t_image[gltf_material.normalTexture.index].getImageView();
						samplers[2].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}

					if (gltf_material.occlusionTexture.index >= 0)
					{
						samplers[3].sampler = model->t_image[gltf_material.occlusionTexture.index].getSampler();
						samplers[3].imageView = model->t_image[gltf_material.occlusionTexture.index].getImageView();
						samplers[3].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}
					if (gltf_material.emissiveTexture.index >= 0)
					{
						samplers[4].sampler = model->t_image[gltf_material.emissiveTexture.index].getSampler();
						samplers[4].imageView = model->t_image[gltf_material.emissiveTexture.index].getImageView();
						samplers[4].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}

					vk::WriteDescriptorSet write[] =
					{
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(array_length(uniforms))
						.setPBufferInfo(uniforms)
						.setDstBinding(0)
						.setDstSet(model->m_DS_material[i].get()),
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
						.setDescriptorCount(array_length(samplers))
						.setPImageInfo(samplers.data())
						.setDstBinding(10)
						.setDstSet(model->m_DS_material[i].get()),
					};
					ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
				}
			}


		}
		model->m_meshes.resize(gltf_model.meshes.size());
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			auto& gltf_mesh = gltf_model.meshes[mesh_index];
			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
			{
				tinygltf::Primitive primitive = gltf_mesh.primitives[i];
				tinygltf::Accessor indexAccessor = gltf_model.accessors[primitive.indices];
				tinygltf::BufferView bufferview_index = gltf_model.bufferViews[indexAccessor.bufferView];

				for (auto& attrib : primitive.attributes)
				{
					tinygltf::Accessor accessor = gltf_model.accessors[attrib.second];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					int size = 1;
					if (accessor.type != TINYGLTF_TYPE_SCALAR)
					{
						size = accessor.type;
					}

					int vaa = -1;
					if (attrib.first.compare("POSITION") == 0) vaa = 0;
					if (attrib.first.compare("NORMAL") == 0) vaa = 1;
					if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
					if (vaa > -1)
					{
						model->m_meshes[mesh_index].vib_disc.push_back(vk::VertexInputBindingDescription().setBinding(vaa).setInputRate(vk::VertexInputRate::eVertex).setStride(accessor.ByteStride(bufferview)));
						model->m_meshes[mesh_index].vid_disc.push_back(vk::VertexInputAttributeDescription().setBinding(vaa).setLocation(vaa).setFormat(GLtoVK::toFormat(size, accessor.componentType)).setOffset(accessor.byteOffset));
					}
					else
					{
						std::cout << "vaa missing: " << attrib.first << std::endl;
					}
				}

			}
		}

		model->gltf_model = std::move(gltf_model);
		return model;
	}
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
		// 		{
		// 			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
		// 			vk::DescriptorSetLayoutBinding binding[] = {
		// 				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
		// 				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
		// 				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
		// 				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
		// 				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
		// 				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
		// 				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
		// 				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eUniformBuffer, 1, stage),
		// 			};
		// 			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		// 			desc_layout_info.setBindingCount(array_length(binding));
		// 			desc_layout_info.setPBindings(binding);
		// 			m_DSL[DSL_Renderer] = ctx.m_ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		// 		}

				// descriptor set
		{
			// 			vk::DescriptorSetLayout layouts[] =
			// 			{
			// 				m_DSL[DSL_Renderer].get(),
			// 			};
			// 			vk::DescriptorSetAllocateInfo desc_info;
			// 			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			// 			desc_info.setDescriptorSetCount(array_length(layouts));
			// 			desc_info.setPSetLayouts(layouts);
			// 			m_DS[DS_Renderer] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			//
			// 			vk::DescriptorBufferInfo uniforms[] =
			// 			{
			// 				u_info.getInfo(),
			// 			};
			// 			vk::DescriptorBufferInfo storages[] =
			// 			{
			// 				b_hashmap.getInfo(),
			// 				b_hashmap_mask.getInfo(),
			// 				b_interior_counter.getInfo(),
			// 				b_leaf_counter.getInfo(),
			// 				b_interior.getInfo(),
			// 				b_leaf.getInfo(),
			// 			};
			// 
			// 			vk::WriteDescriptorSet write[] =
			// 			{
			// 				vk::WriteDescriptorSet()
			// 				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			// 				.setDescriptorCount(array_length(uniforms))
			// 				.setPBufferInfo(uniforms)
			// 				.setDstBinding(0)
			// 				.setDstSet(m_DS[DS_Renderer].get()),
			// 				vk::WriteDescriptorSet()
			// 				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			// 				.setDescriptorCount(array_length(storages))
			// 				.setPBufferInfo(storages)
			// 				.setDstBinding(1)
			// 				.setDstSet(m_DS[DS_Renderer].get()),
			// 			};
			// 			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					//					m_DSL[DSL_Renderer].get(),
										sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
										ctx.m_DSL[Context::DSL_Model_Material].get(),
										ctx.m_DSL[Context::DSL_Scene].get(),
										//					RenderTarget::s_descriptor_set_layout.get(),
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
				{"ModelRender.vert.spv", vk::ShaderStageFlagBits::eVertex},
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
				vk::Viewport(0.f, 0.f, rt.m_resolution.width, rt.m_resolution.height, 0.f, 1.f),
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
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vk::VertexInputBindingDescription vi_binding[] =
			{
				vk::VertexInputBindingDescription().setBinding(0).setStride(12).setInputRate(vk::VertexInputRate::eVertex),
				vk::VertexInputBindingDescription().setBinding(1).setStride(12).setInputRate(vk::VertexInputRate::eVertex),
				vk::VertexInputBindingDescription().setBinding(2).setStride(8).setInputRate(vk::VertexInputRate::eVertex),
			};
			vk::VertexInputAttributeDescription vi_attrib[] =
			{
				vk::VertexInputAttributeDescription().setLocation(0).setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
				vk::VertexInputAttributeDescription().setLocation(1).setBinding(1).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
				vk::VertexInputAttributeDescription().setLocation(2).setBinding(2).setFormat(vk::Format::eR32G32Sfloat).setOffset(0),
			};
			vertex_input_info.vertexBindingDescriptionCount = array_size(vi_binding);
			vertex_input_info.pVertexBindingDescriptions = vi_binding;
			vertex_input_info.vertexAttributeDescriptionCount = array_size(vi_attrib);
			vertex_input_info.pVertexAttributeDescriptions = vi_attrib;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
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
		//		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { m_DS[DSL_Renderer].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 2, { m_ctx->m_DS_Scene.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		auto& gltf_model = model.gltf_model;
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			auto& gltf_mesh = gltf_model.meshes[mesh_index];

			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
			{
				tinygltf::Primitive primitive = gltf_mesh.primitives[i];
				for (auto& attrib : primitive.attributes)
				{
					tinygltf::Accessor accessor = gltf_model.accessors[attrib.second];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					int vaa = -1;
					if (attrib.first.compare("POSITION") == 0) vaa = 0;
					if (attrib.first.compare("NORMAL") == 0) vaa = 1;
					if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;

					assert(vaa >= 0);
					if (vaa > -1)
					{
						cmd.bindVertexBuffers(vaa, model.b_vertex[bufferview.buffer].getInfo().buffer, model.b_vertex[bufferview.buffer].getInfo().offset + bufferview.byteOffset);
					}
				}

				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 1, { model.m_DS_material[primitive.material].get() }, {});
				{
					tinygltf::Accessor accessor = gltf_model.accessors[primitive.indices];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					vk::IndexType indextype = vk::IndexType::eUint32;
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: indextype = vk::IndexType::eUint32; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indextype = vk::IndexType::eUint16; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: indextype = vk::IndexType::eUint8EXT; break;
					default: assert(false); break;
					}
					cmd.bindIndexBuffer(model.b_vertex[bufferview.buffer].getInfo().buffer, model.b_vertex[bufferview.buffer].getInfo().offset + bufferview.byteOffset, indextype);

					cmd.drawIndexed(accessor.count, 1, 0, 0, 0);

				}
			}
		}


		cmd.endRenderPass();

	}
};
