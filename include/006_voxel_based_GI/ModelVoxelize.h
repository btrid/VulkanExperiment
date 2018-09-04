#pragma once
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <006_voxel_based_GI/VoxelPipeline.h>

struct ModelVoxelize : public Voxelize
{
	struct Resource
	{
		btr::BufferMemory m_vertex;
		btr::BufferMemory m_index;
		btr::BufferMemory m_material;
		btr::BufferMemory m_mesh_info;
		btr::BufferMemory m_indirect;
		uint32_t m_mesh_count;
		uint32_t m_index_count;

		vk::UniqueDescriptorSet m_model_descriptor_set;
	};


	struct Vertex
	{
		glm::vec3 pos;
	};
	struct Mesh
	{
		std::vector<Vertex> vertex;
		std::vector<glm::uvec3> index;
		uint32_t m_material_index;
	};
	struct Material
	{
		vec4 albedo;
		vec4 emission;
	};
	struct MeshInfo
	{
		uint32_t material_index;
	};

	struct Model
	{
		std::vector<Mesh> m_mesh;
		std::array<Material, 16> m_material;
	};

	enum SHADER
	{
		SHADER_VERTEX_VOXELIZE,
		SHADER_GEOMETRY_VOXELIZE,
		SHADER_FRAGMENT_VOXELIZE,
		SHADER_COPY_VOXEL,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_VOXEL,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_VOXEL,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_MAKE_VOXEL,
		PIPELINE_LAYOUT_COPY_VOXEL,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_MAKE_VOXEL,
		PIPELINE_COPY_VOXEL,
		PIPELINE_NUM,
	};

	vk::UniqueRenderPass m_make_voxel_pass;
	vk::UniqueFramebuffer m_make_voxel_framebuffer;

	std::vector<std::shared_ptr<Resource>> m_model_list;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	std::vector<vk::UniqueCommandBuffer> m_make_cmd;

	// model用
	vk::UniqueDescriptorSetLayout m_model_descriptor_set_layout;
	vk::UniqueDescriptorPool m_model_descriptor_pool;


	vk::UniqueImage m_voxel_image;
	vk::UniqueImageView m_voxel_imageview;
	vk::UniqueDeviceMemory m_voxel_imagememory;

	void setup(std::shared_ptr<btr::Context>& context, VoxelPipeline const * const parent)
	{
		auto& gpu = context->m_gpu;
		auto& device = gpu.getDevice();

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);


		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e3D;
			image_info.format = vk::Format::eR32Uint;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { parent->m_voxelize_info_cpu.u_cell_num.x, parent->m_voxelize_info_cpu.u_cell_num.y, parent->m_voxelize_info_cpu.u_cell_num.z };
			auto image = device->createImageUnique(image_info);

			vk::MemoryRequirements memory_request = device->getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			auto image_memory = device->allocateMemoryUnique(memory_alloc_info);
			device->bindImageMemory(image.get(), image_memory.get(), 0);

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;

			{
				// 初期設定
				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eGeneral;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = subresourceRange;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e3D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = vk::Format::eR32Uint;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eNearest;
			sampler_info.minFilter = vk::Filter::eNearest;
			sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
			sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.mipLodBias = 0.0f;
			sampler_info.compareOp = vk::CompareOp::eNever;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = 0.f;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

			m_voxel_image = std::move(image);
			m_voxel_imagememory = std::move(image_memory);
			m_voxel_imageview = device->createImageViewUnique(view_info);

		}

		// renderpass
		{
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(0);
			subpass.setPColorAttachments(nullptr);
			subpass.setPDepthStencilAttachment(nullptr);

			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(0)
				.setPAttachments(nullptr)
				.setSubpassCount(1)
				.setPSubpasses(&subpass);
			m_make_voxel_pass = context->m_device->createRenderPassUnique(renderpass_info);

			{
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_make_voxel_pass.get());
				framebuffer_info.setAttachmentCount(0);
				framebuffer_info.setPAttachments(nullptr);
				framebuffer_info.setWidth(parent->getVoxelInfo().u_cell_num.x);
				framebuffer_info.setHeight(parent->getVoxelInfo().u_cell_num.z);
				framebuffer_info.setLayers(3);
				m_make_voxel_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
			}
		}

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "ModelVoxelize.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "ModelVoxelize.geom.spv",vk::ShaderStageFlagBits::eGeometry },
				{ "ModelVoxelize.frag.spv",vk::ShaderStageFlagBits::eFragment },
				{ "CopyVoxel.comp.spv",vk::ShaderStageFlagBits::eCompute },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_list[i] = std::move(loadShaderUnique(device.getHandle(), path + shader_info[i].name));
				m_stage_info[i].setStage(shader_info[i].stage);
				m_stage_info[i].setModule(m_shader_list[i].get());
				m_stage_info[i].setPName("main");
			}
		}

		// Create descriptor set layout
		{
			{
				std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
				bindings[DESCRIPTOR_SET_LAYOUT_VOXEL] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(1)
					.setBinding(0),
				};
				for (u32 i = 0; i < bindings.size(); i++)
				{
					vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
						.setBindingCount(bindings[i].size())
						.setPBindings(bindings[i].data());
					m_descriptor_set_layout[i] = device->createDescriptorSetLayoutUnique(descriptor_layout_info);
				}
			}

			// descriptor set
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOXEL].get(),
				};
				vk::DescriptorSetAllocateInfo alloc_info;
				alloc_info.descriptorPool = context->m_descriptor_pool.get();
				alloc_info.descriptorSetCount = array_length(layouts);
				alloc_info.pSetLayouts = layouts;
				m_descriptor_set[DESCRIPTOR_SET_VOXEL] = std::move(device->allocateDescriptorSetsUnique(alloc_info)[0]);

				auto param = vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_voxel_imageview.get());
				std::vector<vk::DescriptorImageInfo> textures(1, param);
				std::vector<vk::WriteDescriptorSet> write_descriptor_set =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(textures.size())
					.setPImageInfo(textures.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOXEL].get()),
				};
				device->updateDescriptorSets(write_descriptor_set, {});
			}

	
		}

		// setup descriptor set layout
		{
			vk::DescriptorSetLayoutCreateInfo layout_info;
			vk::DescriptorSetLayoutBinding bindings[2];
			bindings[0].setDescriptorType(vk::DescriptorType::eStorageBuffer);
			bindings[0].setBinding(0);
			bindings[0].setDescriptorCount(1);
			bindings[0].setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
			bindings[1].setDescriptorType(vk::DescriptorType::eStorageBuffer);
			bindings[1].setBinding(1);
			bindings[1].setDescriptorCount(1);
			bindings[1].setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
			layout_info.setBindingCount(array_length(bindings));
			layout_info.setPBindings(bindings);
			m_model_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info);
		}

		// setup pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					parent->getDescriptorSetLayout(VoxelPipeline::DESCRIPTOR_SET_LAYOUT_VOXEL),
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOXEL].get(),
					m_model_descriptor_set_layout.get()
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					parent->getDescriptorSetLayout(VoxelPipeline::DESCRIPTOR_SET_LAYOUT_VOXEL),
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOXEL].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PIPELINE_LAYOUT_COPY_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);

			}
		}

		// setup pipeline
		{
			{
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(m_stage_info[SHADER_COPY_VOXEL])
					.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COPY_VOXEL].get()),
				};
				auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
				m_pipeline[PIPELINE_COPY_VOXEL] = std::move(compute_pipeline[0]);

			}
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Extent3D size;
			size.setWidth(parent->getVoxelInfo().u_cell_num.x);
			size.setHeight(parent->getVoxelInfo().u_cell_num.y);
			size.setDepth(parent->getVoxelInfo().u_cell_num.z);
			vk::Viewport viewports[] = {
				vk::Viewport(0.f, 0.f, (float)size.depth, (float)size.height, 0.f, 1.f),
				vk::Viewport(0.f, 0.f, (float)size.width, (float)size.depth, 0.f, 1.f),
				vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f),
			};
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.depth, size.height)),
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.depth)),
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height)),
			};
			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(array_length(viewports));
			viewport_info.setPViewports(viewports);
			viewport_info.setScissorCount((uint32_t)scissor.size());
			viewport_info.setPScissors(scissor.data());

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setLineWidth(1.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			// ブレンド
			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(Vertex))
			};

			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				// pos
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(0),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::array<vk::PipelineShaderStageCreateInfo, 3> stage_info =
			{
				m_stage_info[SHADER_VERTEX_VOXELIZE],
				m_stage_info[SHADER_GEOMETRY_VOXELIZE],
				m_stage_info[SHADER_FRAGMENT_VOXELIZE],
			};
			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)stage_info.size())
				.setPStages(stage_info.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get())
				.setRenderPass(m_make_voxel_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline[PIPELINE_MAKE_VOXEL] = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}


		// model用
		{
			vk::DescriptorPoolSize pool_size[1];
			pool_size[0].setDescriptorCount(20);
			pool_size[0].setType(vk::DescriptorType::eStorageBuffer);
			vk::DescriptorPoolCreateInfo descriptor_pool_info;
			descriptor_pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
			descriptor_pool_info.setMaxSets(20);
			descriptor_pool_info.setPoolSizeCount(array_length(pool_size));
			descriptor_pool_info.setPPoolSizes(pool_size);
			m_model_descriptor_pool = device->createDescriptorPoolUnique(descriptor_pool_info);

		}

	}

	void draw(std::shared_ptr<btr::Context>& context, VoxelPipeline const * const parent, vk::CommandBuffer cmd)
	{
		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(1);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);
		{
			// clear
			{
				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = m_voxel_image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eGeneral;
				to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
			}

			vk::ClearColorValue clear_value;
			clear_value.setUint32(std::array<uint32_t, 4>{0u});
//			clear_value.setUint32(std::array<uint32_t, 4>{0xFFFFFFFFu});
			cmd.clearColorImage(m_voxel_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, range);
		}


		{
			{
				vk::ImageMemoryBarrier to_shader_barrier;
				to_shader_barrier.image = m_voxel_image.get();
				to_shader_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_barrier.newLayout = vk::ImageLayout::eGeneral;
				to_shader_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_shader_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite;
				to_shader_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_barrier });
			}

			// make voxel
			vk::RenderPassBeginInfo begin_render_info;
			begin_render_info.setFramebuffer(m_make_voxel_framebuffer.get());
			begin_render_info.setRenderPass(m_make_voxel_pass.get());
			begin_render_info.setRenderArea(vk::Rect2D({}, { parent->getVoxelInfo().u_cell_num.x, parent->getVoxelInfo().u_cell_num.z }));

			cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_MAKE_VOXEL].get());

			std::vector<vk::DescriptorSet> descriptor_sets = {
				parent->getDescriptorSet(VoxelPipeline::DESCRIPTOR_SET_VOXEL),
				m_descriptor_set[DESCRIPTOR_SET_VOXEL].get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get(), 0, descriptor_sets, {});

			for (auto& model : m_model_list)
			{
				std::vector<vk::DescriptorSet> descriptor = {
					model->m_model_descriptor_set.get(),
				};
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get(), 2, descriptor, {});
				cmd.bindIndexBuffer(model->m_index.getBufferInfo().buffer, model->m_index.getBufferInfo().offset, vk::IndexType::eUint32);
				cmd.bindVertexBuffers(0, model->m_vertex.getBufferInfo().buffer, model->m_vertex.getBufferInfo().offset);
				cmd.drawIndexedIndirect(model->m_indirect.getBufferInfo().buffer, model->m_indirect.getBufferInfo().offset, model->m_mesh_count, sizeof(vk::DrawIndexedIndirectCommand));
			}
			cmd.endRenderPass();
		}

		{
			// r32uiに構築したvoxelをrgba32fに展開
			{
				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = m_voxel_image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eGeneral;
				to_copy_barrier.newLayout = vk::ImageLayout::eGeneral;
				to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_copy_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COPY_VOXEL].get());
			std::vector<vk::DescriptorSet> descriptor_sets = {
				parent->getDescriptorSet(VoxelPipeline::DESCRIPTOR_SET_VOXEL),
				m_descriptor_set[DESCRIPTOR_SET_VOXEL].get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_COPY_VOXEL].get(), 0, descriptor_sets, {});

			auto group = parent->m_voxelize_info_cpu.u_cell_num;
			cmd.dispatch(group.x, group.y, group.z);
		}

	}
	void addModel(std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd, const Model& model)
	{
		std::vector<Vertex> vertex;
		std::vector<glm::uvec3> index;
		std::vector<MeshInfo> mesh_info;
		std::vector<vk::DrawIndexedIndirectCommand> indirect;
		mesh_info.reserve(model.m_mesh.size());
		indirect.reserve(model.m_mesh.size());

		size_t index_offset = 0;
		size_t vertex_offset = 0;
		for (auto& mesh : model.m_mesh)
		{
			vertex.insert(vertex.end(), mesh.vertex.begin(), mesh.vertex.end());
			auto idx = mesh.index;
			std::for_each(idx.begin(), idx.end(), [=](auto& i) {i += vertex_offset; });
			index.insert(index.end(), idx.begin(), idx.end());

			vk::DrawIndexedIndirectCommand draw_cmd;
			draw_cmd.setFirstIndex(index_offset);
			draw_cmd.setFirstInstance(0);
			draw_cmd.setIndexCount(mesh.index.size() * 3);
			draw_cmd.setInstanceCount(1);
			draw_cmd.setVertexOffset(0);
			indirect.push_back(draw_cmd);

			MeshInfo minfo;
			minfo.material_index = mesh.m_material_index;
			mesh_info.push_back(minfo);

			index_offset += mesh.index.size() * 3;
			vertex_offset += mesh.vertex.size();
		}
		std::shared_ptr<Resource> resource(std::make_shared<Resource>());
		resource->m_mesh_count = model.m_mesh.size();
		resource->m_index_count = index_offset;
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = vector_sizeof(vertex);
			resource->m_vertex = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, vertex.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_vertex.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_vertex.getBufferInfo().buffer, copy);
		}

		{
			btr::BufferMemoryDescriptor desc;
			desc.size = vector_sizeof(index);
			resource->m_index = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, index.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_index.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_index.getBufferInfo().buffer, copy);
		}
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = vector_sizeof(indirect);
			resource->m_indirect = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, indirect.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_indirect.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_indirect.getBufferInfo().buffer, copy);
		}

		{
			btr::BufferMemoryDescriptor desc;
			desc.size = vector_sizeof(mesh_info);
			resource->m_mesh_info = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, mesh_info.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_mesh_info.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_mesh_info.getBufferInfo().buffer, copy);
		}
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = vector_sizeof(model.m_material);
			resource->m_material = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, model.m_material.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_material.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_material.getBufferInfo().buffer, copy);
		}


		{
			vk::DescriptorSetLayout layouts[] = {
				m_model_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo info;
			info.setDescriptorPool(m_model_descriptor_pool.get());
			info.setDescriptorSetCount(array_length(layouts));
			info.setPSetLayouts(layouts);
			resource->m_model_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				resource->m_material.getBufferInfo(),
				resource->m_mesh_info.getBufferInfo(),
			};
			vk::WriteDescriptorSet write_desc;
			write_desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			write_desc.setDescriptorCount(array_length(storages));
			write_desc.setPBufferInfo(storages);
			write_desc.setDstSet(resource->m_model_descriptor_set.get());
			write_desc.setDstBinding(0);
			context->m_device->updateDescriptorSets(write_desc, {});
		}
		m_model_list.push_back(resource);
	}

};
