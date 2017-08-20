#pragma once
#include <memory>
#include <array>
#include <vector>
#include <btrlib/Define.h>
#include <btrlib/Loader.h>
#include <btrlib/cCamera.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/sGlobal.h>
#include <applib/Geometry.h>
#include <applib/sCameraManager.h>

struct VolumeScene
{	
	vec4 u_volume_min;
	vec4 u_volume_max;

	vec4 u_light_pos;
	vec4 u_light_color;
};
struct volumeRenderer
{
	enum
	{
		SHADER_VERTEX_VOLUME,
		SHADER_FRAGMENT_VOLUME,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_VOLUME_SCENE,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_DRAW_VOLUME,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_VOLUME,
		PIPELINE_NUM,
	};

	btr::AllocatedMemory m_volume_scene_gpu;
	VolumeScene m_volume_scene_cpu;

	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::Pipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::DescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	vk::Image m_volume_image;
	vk::ImageView m_volume_image_view;
	vk::DeviceMemory m_volume_image_memory;
	vk::Sampler m_volume_sampler;

public:
	volumeRenderer()
	{}

	void setup(std::shared_ptr<btr::Loader>& loader)
	{
//		m_volume_scene_cpu.u_background_color = 
		m_volume_scene_cpu.u_volume_min = vec4(-1000.);
		m_volume_scene_cpu.u_volume_max = vec4(1000.);
		m_volume_scene_cpu.u_light_pos = vec4(1000.);
		m_volume_scene_cpu.u_light_color = vec4(1., 0., 0., 1.);

		{
			glm::uvec3 texSize(32);
			std::vector<glm::vec1> rgba(texSize.x*texSize.y*texSize.z);
			for (int z = 0; z < texSize.z; z++)
			{
				for (int y = 0; y < texSize.y; y++)
				{
					for (int x = 0; x < texSize.x; x++)
					{
						for (int w = 0; w < 1; w++)
						{
							float a = glm::simplex(glm::vec3(x, y, z)*0.08f) * 0.06f;
							a = glm::max(a, 0.001f);
							rgba.data()[x + y*texSize.x + z*texSize.x*texSize.y][w] = a;

						}
					}
				}
			}
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e3D;
			image_info.format = vk::Format::eR32Sfloat;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { texSize.x, texSize.y, texSize.z };
			m_volume_image = loader->m_device->createImage(image_info);

			vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(m_volume_image);
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
			m_volume_image_memory = loader->m_device->allocateMemory(memory_alloc_info);
			loader->m_device->bindImageMemory(m_volume_image, m_volume_image_memory, 0);

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
			m_volume_sampler = loader->m_device->createSampler(sampler_info);

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;
			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e3D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = m_volume_image;
			view_info.subresourceRange = subresourceRange;
			m_volume_image_view = loader->m_device->createImageView(view_info);

			{

				vk::ImageMemoryBarrier to_transfer;
				to_transfer.image = m_volume_image;
				to_transfer.oldLayout = vk::ImageLayout::eUndefined;
				to_transfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_transfer.subresourceRange = subresourceRange;
				loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_transfer });

				vk::ImageSubresourceLayers l;
				l.setAspectMask(vk::ImageAspectFlagBits::eColor);
				l.setBaseArrayLayer(0);
				l.setLayerCount(1);
				l.setMipLevel(0);
				{
					btr::BufferMemory::Descriptor desc;
					desc.size = memory_request.size;
					desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
					auto staging = loader->m_staging_memory.allocateMemory(desc);
					memcpy(staging.getMappedPtr(), rgba.data(), desc.size);
					vk::BufferImageCopy copy;
					copy.setBufferOffset(staging.getBufferInfo().offset);
					copy.setImageSubresource(l);
					copy.setImageExtent(image_info.extent);

					loader->m_cmd.copyBufferToImage(staging.getBufferInfo().buffer, m_volume_image, vk::ImageLayout::eTransferDstOptimal, copy);
				}

				vk::ImageMemoryBarrier to_shader_read;
				to_shader_read.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read.image = m_volume_image;
				to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read.subresourceRange = subresourceRange;
				loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read });

			}

		}

		{
			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(VolumeScene);
			m_volume_scene_gpu = loader->m_uniform_memory.allocateMemory(desc);
		}


		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "VolumeRender.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "VolumeRender.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_info[i].name));
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		// setup descriptor_set_layout
		{
			std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
			bindings[DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE] =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setBinding(1),
			};
			for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
			{
				vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
					.setBindingCount(bindings[i].size())
					.setPBindings(bindings[i].data());
				m_descriptor_set_layout[i] = loader->m_device->createDescriptorSetLayout(descriptor_set_layout_info);
			}

		}
		// setup descriptor_set
		{
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = loader->m_descriptor_pool;
			alloc_info.descriptorSetCount = m_descriptor_set_layout.size();
			alloc_info.pSetLayouts = m_descriptor_set_layout.data();
			auto descriptor_set = loader->m_device->allocateDescriptorSets(alloc_info);
			std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
			{

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_volume_scene_gpu.getBufferInfo(),
				};
				std::vector<vk::DescriptorImageInfo> images = {
					vk::DescriptorImageInfo().setSampler(m_volume_sampler).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(m_volume_image_view),
				};
				std::vector<vk::WriteDescriptorSet> write_desc =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(images.size())
					.setPImageInfo(images.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE]),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(uniforms.size())
					.setPBufferInfo(uniforms.data())
					.setDstBinding(1)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE]),
				};
				loader->m_device->updateDescriptorSets(write_desc, {});
			}

		}

		// setup pipeline_layout
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE],
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME] = loader->m_device->createPipelineLayout(pipeline_layout_info);
		}

		// setup pipeline
		vk::Extent3D size;
		size.setWidth(640);
		size.setHeight(480);
		size.setDepth(1);
		// pipeline
// 		{
// 			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
// 			{
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(m_shader_info[SHADER_COMPUTE_MAP_UPDATE])
// 				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR]),
// 			};
// 			auto pipelines = loader->m_device->createComputePipelines(loader->m_cache, compute_pipeline_info);
// 			m_pipeline[PIPELINE_COMPUTE_MAP] = pipelines[0];
// 		}
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount((uint32_t)scissor.size());
			viewportInfo.setPScissors(scissor.data());

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

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				m_shader_info[SHADER_VERTEX_VOLUME],
				m_shader_info[SHADER_FRAGMENT_VOLUME],
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(2)
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME])
				.setRenderPass(loader->m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = loader->m_device->createGraphicsPipelines(loader->m_cache, graphics_pipeline_info);
			std::copy(pipelines.begin(), pipelines.end(), m_pipeline.begin());

		}

	}
	void execute(std::shared_ptr<btr::Executer>& executer)
	{
		executer->m_cmd;
		{
			auto& keyboard = executer->m_window->getInput().m_keyboard;
			float value = 30.f;
			vec4 lightPos(0.f);
			if (keyboard.isHold(VK_LEFT)) {
				lightPos.x -= value;
			}
			if (keyboard.isHold(VK_RIGHT)) {
				lightPos.x += value;
			}
			if (keyboard.isHold(VK_UP)) {
				lightPos.z += value;
			}
			if (keyboard.isHold(VK_DOWN)) {
				lightPos.z -= value;
			}
			if (keyboard.isHold('a')) {
				lightPos.y += value;
			}
			if (keyboard.isHold('b')) {
				lightPos.y -= value;
			}
			m_volume_scene_cpu.u_light_pos += lightPos;

			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(VolumeScene);
			desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = executer->m_staging_memory.allocateMemory(desc);
			*staging.getMappedPtr<VolumeScene>() = m_volume_scene_cpu;
			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getBufferInfo().offset);
			copy.setDstOffset(m_volume_scene_gpu.getBufferInfo().offset);
			copy.setSize(desc.size);


			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;
			vk::ImageMemoryBarrier to_trans_write;
			to_trans_write.image = m_volume_image;
			to_trans_write.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_trans_write.newLayout = vk::ImageLayout::eTransferDstOptimal;
			to_trans_write.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_trans_write.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			to_trans_write.subresourceRange = subresourceRange;
			executer->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_trans_write });

			executer->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_volume_scene_gpu.getBufferInfo().buffer, copy);

			vk::ImageMemoryBarrier to_shader_read;
			to_shader_read.image = m_volume_image;
			to_shader_read.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			to_shader_read.subresourceRange = subresourceRange;
			executer->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read });
		}
	}

	void draw(vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_VOLUME]);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME], 0, m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE], {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME], 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
		cmd.draw(4, 1, 0, 0);

	}
};



