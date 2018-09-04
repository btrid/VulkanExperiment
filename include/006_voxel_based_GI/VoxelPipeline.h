#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/sCameraManager.h>

struct VoxelInfo
{
	vec4 u_area_min;
	vec4 u_area_max;
	vec4 u_cell_size;
	uvec4 u_cell_num;

	VoxelInfo()
	{
		u_area_max = vec4(1000.f);
		u_area_min = vec4(-1000.f);
		u_cell_num = uvec4(32);
		u_cell_size = (u_area_max - u_area_min) / vec4(u_cell_num);
	}
};

struct VoxelPipeline;
struct Voxelize
{
	virtual void draw(std::shared_ptr<btr::Context>& context, VoxelPipeline const * const parent, vk::CommandBuffer cmd) = 0;
	virtual ~Voxelize() = default;
};

struct VoxelPipeline
{
	enum SHADER
	{
		SHADER_DRAW_VOXEL_VERTEX,
		SHADER_DRAW_VOXEL_FRAGMENT,
		SHADER_MAKE_VOXEL_HIERARCHY_COMPUTE,
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
		PIPELINE_LAYOUT_DRAW_VOXEL,
		PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_VOXEL,
		PIPELINE_MAKE_HIERARCHY_VOXEL,
		PIPELINE_NUM,
	};

	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	VoxelInfo m_voxelize_info_cpu;
	btr::BufferMemory m_voxel_info;

	vk::UniqueImage m_voxel_hierarchy_image;
	vk::UniqueDeviceMemory m_voxel_hierarchy_imagememory;
	std::vector<vk::UniqueImageView> m_voxel_hierarchy_imageview;
	vk::UniqueImageView m_voxel_imageview;
	vk::UniqueSampler m_voxel_sampler;

	std::vector<std::shared_ptr<Voxelize>> m_voxelize_list;
	void setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target, const VoxelInfo& info)
	{
		m_render_target = render_target;

		auto& gpu = context->m_gpu;
		auto& device = gpu.getDevice();

		m_voxelize_info_cpu = info;
		int mipnum = 5;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		// レンダーパス
		{
			// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);
			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(render_target->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);

			{
				vk::ImageView view[] = {
					render_target->m_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(render_target->m_info.extent.width);
				framebuffer_info.setHeight(render_target->m_info.extent.height);
				framebuffer_info.setLayers(1);
				m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
			}
		}

		// resource setup
		{
			btr::BufferMemoryDescriptor desc;
			desc.size = sizeof(VoxelInfo);
			m_voxel_info = context->m_uniform_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			*staging.getMappedPtr<VoxelInfo>() = m_voxelize_info_cpu;

			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getBufferInfo().offset);
			copy.setDstOffset(m_voxel_info.getBufferInfo().offset);
			copy.setSize(desc.size);
			cmd.copyBuffer(staging.getBufferInfo().buffer, m_voxel_info.getBufferInfo().buffer, copy);
		}


		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e3D;
			image_info.format = vk::Format::eR8G8B8A8Unorm;
			image_info.mipLevels = mipnum;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { m_voxelize_info_cpu.u_cell_num.x, m_voxelize_info_cpu.u_cell_num.y, m_voxelize_info_cpu.u_cell_num.z };
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
			subresourceRange.levelCount = image_info.mipLevels;

			{
				// 初期設定
				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_copy_barrier.subresourceRange = subresourceRange;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_copy_barrier });

			}

			m_voxel_hierarchy_image = std::move(image);
			m_voxel_hierarchy_imagememory = std::move(image_memory);

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e3D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = m_voxel_hierarchy_image.get();
			view_info.subresourceRange = subresourceRange;

			m_voxel_imageview = device->createImageViewUnique(view_info);

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eLinear;
			sampler_info.minFilter = vk::Filter::eLinear;
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

			m_voxel_sampler = context->m_device->createSamplerUnique(sampler_info);

			view_info.subresourceRange.levelCount = 1;
			m_voxel_hierarchy_imageview.resize(mipnum);
			for (int i = 0; i < mipnum; i++)
			{
				view_info.subresourceRange.baseMipLevel = i;
				m_voxel_hierarchy_imageview[i] = device->createImageViewUnique(view_info);
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
				{ "DrawVoxel.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "DrawVoxel.frag.spv",vk::ShaderStageFlagBits::eFragment },
				{ "MakeVoxelHierarchy.comp.spv",vk::ShaderStageFlagBits::eCompute },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceBtrLibPath() + "shader\\binary\\";
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
				std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_NUM);
				bindings[DESCRIPTOR_SET_VOXEL] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(8)
					.setBinding(1),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(1)
					.setBinding(2),
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

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_voxel_info.getBufferInfo(),
				};

				auto param = vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_voxel_hierarchy_imageview[0].get());
				std::vector<vk::DescriptorImageInfo> textures(8, param);
				for (int i = 0; i < mipnum; i++)
				{
					textures[i].setImageView(m_voxel_hierarchy_imageview[i].get());
				}

				std::vector<vk::DescriptorImageInfo> samplers = {
					vk::DescriptorImageInfo()
					.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
					.setImageView(m_voxel_imageview.get())
					.setSampler(m_voxel_sampler.get())
				};

				std::vector<vk::WriteDescriptorSet> write_descriptor_set =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(uniforms.size())
					.setPBufferInfo(uniforms.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOXEL].get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(samplers.size())
					.setPImageInfo(samplers.data())
					.setDstBinding(2)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOXEL].get()),
				};
				for (int i = 0; i < textures.size(); i++)
				{
					vk::WriteDescriptorSet tex = vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eStorageImage)
						.setDescriptorCount(1)
						.setPImageInfo(&textures[i])
						.setDstBinding(1)
						.setDstArrayElement(i)
						.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOXEL].get());
					write_descriptor_set.push_back(tex);
				}
				device->updateDescriptorSets(write_descriptor_set, {});
			}

		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout[DESCRIPTOR_SET_VOXEL].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};
				vk::PushConstantRange constants[] = {
					vk::PushConstantRange()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex)
					.setSize(4)
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
				pipeline_layout_info.setPPushConstantRanges(constants);
				m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout[DESCRIPTOR_SET_VOXEL].get(),
				};
				vk::PushConstantRange constants[] = {
					vk::PushConstantRange()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setSize(4)
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
				pipeline_layout_info.setPPushConstantRanges(constants);
				m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// pipeline
		{
			{
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(m_stage_info[SHADER_MAKE_VOXEL_HIERARCHY_COMPUTE])
					.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL].get()),
				};
				auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
				m_pipeline[PIPELINE_MAKE_HIERARCHY_VOXEL] = std::move(compute_pipeline[0]);

			}

			{
				// assembly
				vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
					.setPrimitiveRestartEnable(VK_FALSE)
					.setTopology(vk::PrimitiveTopology::eTriangleStrip);

				// viewport
				vk::Viewport viewports[] = {
					vk::Viewport(0.f, 0.f, (float)context->m_window->getClientSize().x, (float)context->m_window->getClientSize().y),
				};
				vk::Rect2D scissors[] = {
					vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()),
				};
				vk::PipelineViewportStateCreateInfo viewport_info;
				viewport_info.setViewportCount(array_length(viewports));
				viewport_info.setPViewports(viewports);
				viewport_info.setScissorCount(array_length(scissors));
				viewport_info.setPScissors(scissors);

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
				depth_stencil_info.setDepthTestEnable(VK_TRUE);
				depth_stencil_info.setDepthWriteEnable(VK_TRUE);
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
				depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
				depth_stencil_info.setStencilTestEnable(VK_FALSE);

				// ブレンド
				vk::PipelineColorBlendAttachmentState blend_states[] = {
					vk::PipelineColorBlendAttachmentState()
					.setBlendEnable(VK_FALSE)
					.setColorWriteMask(vk::ColorComponentFlagBits::eR
						| vk::ColorComponentFlagBits::eG
						| vk::ColorComponentFlagBits::eB
						| vk::ColorComponentFlagBits::eA)
				};
				vk::PipelineColorBlendStateCreateInfo blend_info;
				blend_info.setAttachmentCount(array_length(blend_states));
				blend_info.setPAttachments(blend_states);

				vk::PipelineVertexInputStateCreateInfo vertex_input_info;

				vk::PipelineShaderStageCreateInfo stage_infos[] =
				{
					m_stage_info[SHADER_DRAW_VOXEL_VERTEX],
					m_stage_info[SHADER_DRAW_VOXEL_FRAGMENT],
				};
				std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
				{
					vk::GraphicsPipelineCreateInfo()
					.setStageCount(array_length(stage_infos))
					.setPStages(stage_infos)
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&assembly_info)
					.setPViewportState(&viewport_info)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL].get())
					.setRenderPass(m_render_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info),
				};
				m_pipeline[PIPELINE_DRAW_VOXEL] = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
			}

		}

	}

	vk::CommandBuffer make(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(5);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);

		{
			vk::ImageMemoryBarrier to_read_barrier;
			to_read_barrier.image = m_voxel_hierarchy_image.get();
			to_read_barrier.newLayout = vk::ImageLayout::eGeneral;
			to_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			to_read_barrier.subresourceRange = range;
			to_read_barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
		}

		for (auto& voxelize : m_voxelize_list)
		{
			voxelize->draw(context, this, cmd);
		}
		cmd.end();
		return cmd;

	}
	vk::CommandBuffer makeHierarchy(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(1);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);

		vk::ImageMemoryBarrier to_read_barrier;
		to_read_barrier.image = m_voxel_hierarchy_image.get();
		to_read_barrier.oldLayout = vk::ImageLayout::eGeneral;
		to_read_barrier.newLayout = vk::ImageLayout::eGeneral;
		to_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		to_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		to_read_barrier.subresourceRange = range;

		auto groups = m_voxelize_info_cpu.u_cell_num;
		for (int32_t i = 0; i < 5; i++)
		{
			to_read_barrier.subresourceRange.baseMipLevel = i;
			if (i == 0)
			{
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
			}
			else
			{
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_MAKE_HIERARCHY_VOXEL].get());
			vk::ArrayProxy<const vk::DescriptorSet> descriptor_sets = {
				m_descriptor_set[DESCRIPTOR_SET_VOXEL].get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL].get(), 0, descriptor_sets, {});
			cmd.pushConstants<int32_t>(m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL].get(), vk::ShaderStageFlagBits::eCompute, 0, i);
			groups /= 2;
			groups = glm::max(groups, uvec4(1));
			cmd.dispatch(groups.x, groups.y, groups.z);
		}

		cmd.end();
		return cmd;

	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context)
	{

		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(5);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);
		{
			// draw voxel
			{
				vk::ImageMemoryBarrier to_read_barrier;
				to_read_barrier.image = m_voxel_hierarchy_image.get();
				to_read_barrier.oldLayout = vk::ImageLayout::eGeneral;
				to_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_read_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
			}
			vk::RenderPassBeginInfo begin_render_info;
			begin_render_info.setFramebuffer(m_framebuffer.get());
			begin_render_info.setRenderPass(m_render_pass.get());
			begin_render_info.setRenderArea(vk::Rect2D({}, context->m_window->getClientSize<vk::Extent2D>()));

			cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_VOXEL].get());
			std::vector<vk::DescriptorSet> descriptor_sets = {
				m_descriptor_set[DESCRIPTOR_SET_VOXEL].get(),
				sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL].get(), 0, descriptor_sets, {});
			uint32_t miplevel = 0;
			cmd.pushConstants<uint32_t>(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL].get(), vk::ShaderStageFlagBits::eVertex, 0, miplevel);

			cmd.draw(14, m_voxelize_info_cpu.u_cell_num.x *m_voxelize_info_cpu.u_cell_num.y*m_voxelize_info_cpu.u_cell_num.z / (1<<(3*miplevel)), 0, 0);
			cmd.endRenderPass();
		}

		cmd.end();
		return cmd;
	}

	template<typename T, typename... Args>
	std::shared_ptr<T> createPipeline(std::shared_ptr<btr::Context>& context, Args... args)
	{
		auto ptr = std::make_shared<T>();
		m_voxelize_list.push_back(ptr);
		ptr->setup(context, this, args...);
		return ptr;
	}
	const VoxelInfo& getVoxelInfo()const { return m_voxelize_info_cpu; }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout type)const { return m_descriptor_set_layout[type].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet type)const { return m_descriptor_set[type].get(); }
};