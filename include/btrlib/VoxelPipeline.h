#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>

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
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_VOXELIZE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_VOXELIZE,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_DRAW_VOXEL,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_VOXEL,
		PIPELINE_NUM,
	};

	std::shared_ptr<RenderBackbufferModule> m_draw_render_pass;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::UniquePipelineCache m_pipeline_cache;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	VoxelInfo m_voxelize_info_cpu;
	btr::BufferMemory m_voxel_info;
	vk::UniqueImage m_voxel_image;
	vk::UniqueImageView m_voxel_imageview;
	vk::UniqueDeviceMemory m_voxel_imagememory;

	vk::UniqueImage m_voxel_hierarchy_image;
	vk::UniqueImageView m_voxel_hierarchy_imageview;
	vk::UniqueDeviceMemory m_voxel_hierarchy_imagememory;

	std::vector<std::shared_ptr<Voxelize>> m_voxelize_list;
	void setup(std::shared_ptr<btr::Context>& context, const VoxelInfo& info)
	{
		auto& gpu = context->m_gpu;
		auto& device = gpu.getDevice();

		m_voxelize_info_cpu = info;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

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
			cmd->copyBuffer(staging.getBufferInfo().buffer, m_voxel_info.getBufferInfo().buffer, copy);
		}

		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e3D;
			image_info.format = vk::Format::eR32Uint;
			image_info.mipLevels = 1;
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
			subresourceRange.levelCount = 1;

			{
				// 初期設定
				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eGeneral;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = subresourceRange;

				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });

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

		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e3D;
			image_info.format = vk::Format::eR32Sfloat;
			image_info.mipLevels = 4;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { m_voxelize_info_cpu.u_cell_num.x/2, m_voxelize_info_cpu.u_cell_num.y / 2, m_voxelize_info_cpu.u_cell_num.z / 2 };
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
				to_copy_barrier.newLayout = vk::ImageLayout::eGeneral;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = subresourceRange;

				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e3D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			m_voxel_hierarchy_image = std::move(image);
			m_voxel_hierarchy_imagememory = std::move(image_memory);
			m_voxel_hierarchy_imageview = device->createImageViewUnique(view_info);

		}


		// レンダーパス
		{
			m_draw_render_pass = std::make_shared<RenderBackbufferModule>(context);
		}


		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "DrawAlbedoVoxel.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "DrawAlbedoVoxel.frag.spv",vk::ShaderStageFlagBits::eFragment },
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
				bindings[DESCRIPTOR_SET_VOXELIZE] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(1)
					.setBinding(1),
				};
				for (u32 i = 0; i < bindings.size(); i++)
				{
					vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
						.setBindingCount(bindings[i].size())
						.setPBindings(bindings[i].data());
					m_descriptor_set_layout[i] = device->createDescriptorSetLayoutUnique(descriptor_layout_info);
				}
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOXELIZE].get(),
				};
				vk::DescriptorSetAllocateInfo alloc_info;
				alloc_info.descriptorPool = context->m_descriptor_pool.get();
				alloc_info.descriptorSetCount = array_length(layouts);
				alloc_info.pSetLayouts = layouts;
				m_descriptor_set[DESCRIPTOR_SET_VOXELIZE] = std::move(device->allocateDescriptorSetsUnique(alloc_info)[0]);

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_voxel_info.getBufferInfo(),
				};
				std::vector<vk::DescriptorImageInfo> textures = {
					vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_voxel_imageview.get()),
				};
				std::vector<vk::WriteDescriptorSet> write_descriptor_set =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(uniforms.size())
					.setPBufferInfo(uniforms.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOXELIZE].get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(textures.size())
					.setPImageInfo(textures.data())
					.setDstBinding(1)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOXELIZE].get()),
				};
				device->updateDescriptorSets(write_descriptor_set, {});
			}

			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout[DESCRIPTOR_SET_VOXELIZE].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// pipeline
		{
			// キャッシュ
			vk::PipelineCacheCreateInfo pipeline_cache_info = vk::PipelineCacheCreateInfo();
			m_pipeline_cache = device->createPipelineCacheUnique(pipeline_cache_info);

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
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
				depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
				depth_stencil_info.setStencilTestEnable(VK_FALSE);

				// ブレンド
				vk::PipelineColorBlendAttachmentState blend_states[] = {
					vk::PipelineColorBlendAttachmentState()
					.setBlendEnable(VK_FALSE)
					.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
					.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
					.setColorBlendOp(vk::BlendOp::eAdd)
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
					.setRenderPass(m_draw_render_pass->getRenderPass())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info),
				};
				m_pipeline[PIPELINE_DRAW_VOXEL] = std::move(device->createGraphicsPipelinesUnique(m_pipeline_cache.get(), graphics_pipeline_info)[0]);
			}

		}

	}

	vk::CommandBuffer make(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
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
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
			}

			vk::ClearColorValue clear_value;
			clear_value.setUint32(std::array<uint32_t, 4>{});
			cmd.clearColorImage(m_voxel_image.get(), vk::ImageLayout::eTransferDstOptimal, clear_value, range);
			{
				vk::ImageMemoryBarrier to_shader_barrier;
				to_shader_barrier.image = m_voxel_image.get();
				to_shader_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_barrier.newLayout = vk::ImageLayout::eGeneral;
				to_shader_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
				to_shader_barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
				to_shader_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_barrier });
			}
		}

		for (auto& voxelize : m_voxelize_list)
		{
			voxelize->draw(context, this, cmd);
		}
		cmd.end();
		return cmd;

	}
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context)
	{

		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(1);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);
		{
			// draw voxel
			{
				vk::ImageMemoryBarrier to_read_barrier;
				to_read_barrier.image = m_voxel_image.get();
				to_read_barrier.oldLayout = vk::ImageLayout::eGeneral;
				to_read_barrier.newLayout = vk::ImageLayout::eGeneral;
				to_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_read_barrier.subresourceRange = range;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
			}
			vk::RenderPassBeginInfo begin_render_info;
			begin_render_info.setFramebuffer(m_draw_render_pass->getFramebuffer(context->getGPUFrame()));
			begin_render_info.setRenderPass(m_draw_render_pass->getRenderPass());
			begin_render_info.setRenderArea(vk::Rect2D({}, context->m_window->getClientSize<vk::Extent2D>()));

			cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_VOXEL].get());
			std::vector<vk::DescriptorSet> descriptor_sets = {
				m_descriptor_set[DESCRIPTOR_SET_VOXELIZE].get(),
				sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOXEL].get(), 0, descriptor_sets, {});
			cmd.draw(14, m_voxelize_info_cpu.u_cell_num.x *m_voxelize_info_cpu.u_cell_num.y*m_voxelize_info_cpu.u_cell_num.z, 0, 0);
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