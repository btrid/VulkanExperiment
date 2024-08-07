#pragma once
#include <memory>
#include <array>
#include <vector>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <applib/Geometry.h>
#include <applib/sCameraManager.h>
#include <applib/DrawHelper.h>

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

	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;

	btr::BufferMemory m_volume_scene_gpu;
	VolumeScene m_volume_scene_cpu;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	vk::UniqueImage m_volume_image;
	vk::UniqueImageView m_volume_image_view;
	vk::UniqueDeviceMemory m_volume_image_memory;
	vk::UniqueSampler m_volume_sampler;

public:
	volumeRenderer()
	{}

	void setup(std::shared_ptr<btr::Context>& loader)
	{
		auto cmd = loader->m_cmd_pool->allocCmdTempolary(0);
		m_volume_scene_cpu.u_volume_min = vec4(-1000.);
		m_volume_scene_cpu.u_volume_max = vec4(1000.);
		m_volume_scene_cpu.u_light_pos = vec4(0.);
		m_volume_scene_cpu.u_light_color = vec4(1., 0., 0., 1.);

		{
			// レンダーパス
			{
				// sub pass
				std::vector<vk::AttachmentReference> colorRef =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};
				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount((uint32_t)colorRef.size());
				subpass.setPColorAttachments(colorRef.data());

				std::vector<vk::AttachmentDescription> attach_description = {
					// color1
					vk::AttachmentDescription()
					.setFormat(loader->m_window->getSwapchain().m_surface_format.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eDontCare)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
					.setAttachmentCount(attach_description.size())
					.setPAttachments(attach_description.data())
					.setSubpassCount(1)
					.setPSubpasses(&subpass);

				m_render_pass = loader->m_device.createRenderPassUnique(renderpass_info);
			}

			m_framebuffer.resize(loader->m_window->getSwapchain().getBackbufferNum());
			{
				std::array<vk::ImageView, 1> view;

				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass.get());
				framebuffer_info.setAttachmentCount((uint32_t)view.size());
				framebuffer_info.setPAttachments(view.data());
				framebuffer_info.setWidth(loader->m_window->getClientSize().x);
				framebuffer_info.setHeight(loader->m_window->getClientSize().y);
				framebuffer_info.setLayers(1);

				for (size_t i = 0; i < m_framebuffer.size(); i++) {
					view[0] = loader->m_window->getSwapchain().m_backbuffer[i].m_view;
					m_framebuffer[i] = loader->m_device.createFramebufferUnique(framebuffer_info);
				}
			}

		}

		{
			glm::uvec3 texSize(32);
			std::array<float, 32> coef;
			coef.fill(0.9f);
			coef[0] = 0.f;
			coef[1] = 0.1f;
			coef[2] = 0.3f;
			coef[coef.size()-3] = 0.f;
			coef[coef.size()-2] = 0.1f;
			coef[coef.size()-1] = 0.3f;
			std::vector<glm::vec1> rgba(texSize.x*texSize.y*texSize.z);
			for (int z = 0; z < texSize.z; z++)
			{
				for (int y = 0; y < texSize.y; y++)
				{
					for (int x = 0; x < texSize.x; x++)
					{
						for (int w = 0; w < 1; w++)
						{
							float a = glm::simplex(glm::vec3(x, y, z)*0.033f) * (1.f/32.f) * glm::min(glm::min(coef[x], coef[y]), coef[z]);
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
			m_volume_image = loader->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = loader->m_device.getImageMemoryRequirements(m_volume_image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
			m_volume_image_memory = loader->m_device.allocateMemoryUnique(memory_alloc_info);
			loader->m_device.bindImageMemory(m_volume_image.get(), m_volume_image_memory.get(), 0);

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
			m_volume_sampler = loader->m_device.createSamplerUnique(sampler_info);

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
			view_info.image = m_volume_image.get();
			view_info.subresourceRange = subresourceRange;
			m_volume_image_view = loader->m_device.createImageViewUnique(view_info);

			{

				vk::ImageMemoryBarrier to_transfer;
				to_transfer.image = m_volume_image.get();
				to_transfer.oldLayout = vk::ImageLayout::eUndefined;
				to_transfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_transfer.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_transfer });

				vk::ImageSubresourceLayers l;
				l.setAspectMask(vk::ImageAspectFlagBits::eColor);
				l.setBaseArrayLayer(0);
				l.setLayerCount(1);
				l.setMipLevel(0);
				{
					btr::BufferMemoryDescriptor desc;
					desc.size = memory_request.size;
					desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
					auto staging = loader->m_staging_memory.allocateMemory(desc);
					memcpy(staging.getMappedPtr(), rgba.data(), desc.size);
					vk::BufferImageCopy copy;
					copy.setBufferOffset(staging.getInfo().offset);
					copy.setImageSubresource(l);
					copy.setImageExtent(image_info.extent);

					cmd.copyBufferToImage(staging.getInfo().buffer, m_volume_image.get(), vk::ImageLayout::eTransferDstOptimal, copy);
				}

				vk::ImageMemoryBarrier to_shader_read;
				to_shader_read.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
				to_shader_read.image = m_volume_image.get();
				to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read });

			}

		}

		{
			btr::BufferMemoryDescriptor desc;
			desc.size = sizeof(VolumeScene);
			m_volume_scene_gpu = loader->m_uniform_memory.allocateMemory(desc);
			m_volume_scene_gpu.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
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
				m_shader_module[i] = (loadShaderUnique(loader->m_device.get(), path + shader_info[i].name));
				m_shader_info[i].setModule(m_shader_module[i].get());
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
				m_descriptor_set_layout[i] = loader->m_device.createDescriptorSetLayoutUnique(descriptor_set_layout_info);
			}

		}
		// setup descriptor_set
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE].get()
			};
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = loader->m_descriptor_pool.get();
			alloc_info.descriptorSetCount = array_length(layouts);
			alloc_info.pSetLayouts = layouts;
			auto descriptor_set = loader->m_device.allocateDescriptorSetsUnique(alloc_info);
			std::copy(std::make_move_iterator(descriptor_set.begin()), std::make_move_iterator(descriptor_set.end()), m_descriptor_set.begin());
			{

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_volume_scene_gpu.getInfo(),
				};
				std::vector<vk::DescriptorImageInfo> images = {
					vk::DescriptorImageInfo().setSampler(m_volume_sampler.get()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(m_volume_image_view.get()),
				};
				std::vector<vk::WriteDescriptorSet> write_desc =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(images.size())
					.setPImageInfo(images.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE].get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(uniforms.size())
					.setPBufferInfo(uniforms.data())
					.setDstBinding(1)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE].get()),
				};
				loader->m_device.updateDescriptorSets(write_desc, {});
			}

		}

		// setup pipeline_layout
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME] = loader->m_device.createPipelineLayoutUnique(pipeline_layout_info);
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
// 			auto pipelines = loader->m_device.createComputePipelines(loader->m_cache.get(), compute_pipeline_info);
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
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_TRUE)
				.setSrcColorBlendFactor(vk::BlendFactor::eOne)
				.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
				.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setAlphaBlendOp(vk::BlendOp::eAdd)
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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline[PIPELINE_DRAW_VOLUME] = std::move(loader->m_device.createGraphicsPipelinesUnique(loader->m_cache.get(), graphics_pipeline_info)[0]);

		}

	}
	void work(std::shared_ptr<btr::Context>& executer)
	{
		{
			auto& keyboard = executer->m_window->getInput().m_keyboard;
			float value = 10.f;
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
			if (keyboard.isHold('z')) {
				lightPos.y -= value;
			}
			m_volume_scene_cpu.u_light_pos += lightPos;
			printf("%6.2f, %6.2f, %6.2f\n", m_volume_scene_cpu.u_light_pos.x, m_volume_scene_cpu.u_light_pos.y, m_volume_scene_cpu.u_light_pos.z);
			mat4 mat = glm::translate(m_volume_scene_cpu.u_light_pos.xyz()) * glm::scale(vec3(20.f));
			DrawHelper::Order().drawOrder(DrawHelper::SPHERE, DrawCommand{ mat });
		}
	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer)
	{
		vk::CommandBuffer cmd = executer->m_cmd_pool->allocCmdOnetime(0);

		btr::BufferMemoryDescriptor desc;
		desc.size = sizeof(VolumeScene);
		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;

		auto staging = executer->m_staging_memory.allocateMemory(desc);
		*staging.getMappedPtr<VolumeScene>() = m_volume_scene_cpu;
		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_volume_scene_gpu.getInfo().offset);
		copy.setSize(desc.size);

		{
			auto to_write = m_volume_scene_gpu.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_write, {});
		}
		cmd.copyBuffer(staging.getInfo().buffer, m_volume_scene_gpu.getInfo().buffer, copy);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, m_volume_scene_gpu.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead), {});

		vk::RenderPassBeginInfo render_begin_info;
		render_begin_info.setFramebuffer(m_framebuffer[sGlobal::Order().getCurrentFrame()].get());
		render_begin_info.setRenderPass(m_render_pass.get());
		render_begin_info.setRenderArea(vk::Rect2D({}, { 640, 480 }));
		cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eInline);
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_VOLUME].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME].get(), 0, m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE].get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME].get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
		cmd.draw(4, 1, 0, 0);

		cmd.endRenderPass();
		cmd.end();
		return cmd;

	}
};



