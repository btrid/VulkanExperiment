#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

namespace gi2d
{

struct GI2DPM
{
	struct TextureResource
	{
		vk::ImageCreateInfo m_image_info;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;

		vk::ImageSubresourceRange m_subresource_range;
	};

	enum Shader
	{
		ShaderPhotonMapping,
		ShaderRendering,
		ShaderRenderingVS,
		ShaderRenderingFS,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakePRT,
		PipelineLayoutRendering = 0,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakeRT,
		PipelineRendering,
		PipelineRendering_Graphics,
		PipelineNum,
	};

	GI2DPM(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
		m_render_target = render_target;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			auto& tex = m_color_tex;
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR16G16B16A16Sfloat;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { (uint32_t)gi2d_context->RenderWidth, (uint32_t)gi2d_context->RenderHeight, 1 };
			image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;

			tex.m_image = context->m_device->createImageUnique(image_info);
			tex.m_image_info = image_info;

			vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(tex.m_image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_gpu.getHandle(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			tex.m_memory = context->m_device->allocateMemoryUnique(memory_alloc_info);
			context->m_device->bindImageMemory(tex.m_image.get(), tex.m_memory.get(), 0);

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;
			tex.m_subresource_range = subresourceRange;

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = vk::Format::eR16G16B16A16Sfloat;
			view_info.image = tex.m_image.get();
			view_info.subresourceRange = subresourceRange;
			tex.m_image_view = context->m_device->createImageViewUnique(view_info);

			vk::ImageMemoryBarrier to_init;
			to_init.image = m_color_tex.m_image.get();
			to_init.subresourceRange = m_color_tex.m_subresource_range;
			to_init.oldLayout = vk::ImageLayout::eUndefined;
			to_init.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_init.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(),
				0, nullptr, 0, nullptr, 1, &to_init);
		}

		{
			uint32_t size = m_gi2d_context->RenderWidth * m_gi2d_context->RenderHeight / 64;
			b_hit_data = m_context->m_storage_memory.allocateMemory<uint64_t>({ size*3,{} });
		}

		{

			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(1)
					.setBinding(10),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(1)
					.setBinding(11),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
					b_hit_data.getInfo(),
				};
				vk::DescriptorImageInfo output_images[] = {
					vk::DescriptorImageInfo().setImageView(m_color_tex.m_image_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				};
				vk::DescriptorImageInfo samplers[] = 
				{
					vk::DescriptorImageInfo()
					.setImageView(m_color_tex.m_image_view.get())
					.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
					.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
				};

				vk::WriteDescriptorSet write[] = 
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(array_length(samplers))
					.setPImageInfo(samplers)
					.setDstBinding(10)
					.setDstArrayElement(0)
					.setDstSet(m_descriptor_set.get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(array_length(output_images))
					.setPImageInfo(output_images)
					.setDstBinding(11)
					.setDstArrayElement(0)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}

		{
			const char* name[] =
			{
				"PhotonMapping.comp.spv",
				"PhotonCollect.comp.spv",
				"RTRendering.vert.spv",
				"RTRendering.frag.spv",
			};
			static_assert(array_length(name) == ShaderNum, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				gi2d_context->getDescriptorSetLayout(),
				m_descriptor_set_layout.get(),
			};

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setOffset(0).setSize(8).setStageFlags(vk::ShaderStageFlagBits::eCompute),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PipelineLayoutMakePRT] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 2> shader_info;
			shader_info[0].setModule(m_shader[ShaderPhotonMapping].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderRendering].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayoutMakePRT].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayoutRendering].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PipelineMakeRT] = std::move(compute_pipeline[0]);
			m_pipeline[PipelineRendering] = std::move(compute_pipeline[1]);
		}

		// �����_�[�p�X
		{
			vk::AttachmentReference color_ref[] = {
				vk::AttachmentReference()
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setAttachment(0)
			};
			// sub pass
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
		}
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

		{
			vk::PipelineShaderStageCreateInfo shader_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[ShaderRenderingVS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[ShaderRenderingFS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleStrip);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_target->m_resolution.width, (float)render_target->m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), render_target->m_resolution);
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
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
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
			blend_info.setAttachmentCount((uint32_t)blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayoutRendering].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PipelineRendering_Graphics] = std::move(graphics_pipeline[0]);

		}

	}
	void execute(vk::CommandBuffer cmd)
	{
		static vec2 light_pos = vec2(10, 10);
		float move = 0.03f;
		light_pos.x += m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
		light_pos.x -= m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
		light_pos.y -= m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
		light_pos.y += m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;

		uint32_t rt_map_size = (m_gi2d_context->RenderWidth / 16) * (m_gi2d_context->RenderHeight / 16);
		uint32_t rt_map_num = m_gi2d_context->RenderWidth * m_gi2d_context->RenderHeight / 64;
		static int a = 0;
		// ���O�v�Z
		if (a == 0)
		{
			a++;

			{
				vk::BufferMemoryBarrier to_clear[] = {
					m_gi2d_context->b_emission_occlusion.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
					0, nullptr, array_length(to_clear), to_clear, 0, nullptr);

				cmd.fillBuffer(m_gi2d_context->b_emission_occlusion.getInfo().buffer, m_gi2d_context->b_emission_occlusion.getInfo().offset, m_gi2d_context->b_emission_occlusion.getInfo().range, 0);

				vk::BufferMemoryBarrier to_read[] = {
					m_gi2d_context->b_emission_occlusion.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite),
					m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer| vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeRT].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakePRT].get(), 0, m_gi2d_context->getDescriptorSet(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakePRT].get(), 1, m_descriptor_set.get(), {});
			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth / 8, m_gi2d_context->RenderHeight / 8, 1), uvec3(32, 32, 1));
			auto rt_size = m_gi2d_context->RenderSize / 8 / 2;
			auto yy = m_gi2d_context->RenderHeight / 8;
			auto xx = m_gi2d_context->RenderWidth / 8;
			for (int y = 0; y < yy; y++)
			{
				for (int x = 0; x < xx; x++)
				{
					cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutMakePRT].get(), vk::ShaderStageFlagBits::eCompute, 0, ivec2(light_pos));
					cmd.dispatch(1, 1, 1);
				}

			}
		}

		// Lighting
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_gi2d_context->b_light_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_hit_data.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_gi2d_context->b_emission_occlusion.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineRendering].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutRendering].get(), 0, m_gi2d_context->getDescriptorSet(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutRendering].get(), 1, m_descriptor_set.get(), {});

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderWidth, m_gi2d_context->RenderHeight, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// render_target�ɏ���
		{
			{
				vk::ImageMemoryBarrier to_read;
				to_read.setImage(m_color_tex.m_image.get());
				to_read.setSubresourceRange(m_color_tex.m_subresource_range);
				to_read.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				to_read.setOldLayout(vk::ImageLayout::eGeneral);
				to_read.setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands,
					{}, 0, nullptr, 0, nullptr, 1, &to_read);
			}

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
			begin_render_Info.setFramebuffer(m_framebuffer.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineRendering_Graphics].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 0, m_gi2d_context->getDescriptorSet(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 1, m_descriptor_set.get(), {});
			cmd.draw(3, 1, 0, 0);

			cmd.endRenderPass();
		}

	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<RenderTarget> m_render_target;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	TextureResource m_color_tex;
	btr::BufferMemoryEx<uint64_t> b_hit_data;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;


};



}