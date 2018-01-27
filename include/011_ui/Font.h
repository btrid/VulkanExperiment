#pragma once

#include <string>
#include <ft2build.h>
#include <freetype/freetype.h>

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <applib/GraphicsResource.h>
#include <applib/sSystem.h>
struct Font
{
};

struct sFont : SingletonEx <sFont>
{
	FT_Library m_library;

	enum Shader
	{
		SHADER_VERT_RENDER,
		SHADER_FRAG_RENDER,
		SHADER_RASTER_VERT_RENDER,
		SHADER_RASTER_FRAG_RENDER,

		SHADER_NUM,
	};
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	vk::ShaderModule getShaderModle(Shader type) { return m_shader_module[type].get(); }


	enum PipelineLayout
	{
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_RENDER_RASTER,
		PIPELINE_LAYOUT_NUM,
	};
	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_OUTLINE,
		DESCRIPTOR_SET_LAYOUT_RASTER,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;
	vk::PipelineLayout getPipelineLayout(PipelineLayout type) { return m_pipeline_layout[type].get(); }

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM>	m_descriptor_set_layout;

	sFont(const std::shared_ptr<btr::Context>& context)
	{
		// setup shader
		{
			struct
			{
				const char* name;
			}shader_info[] =
			{
				{ "FontRender.vert.spv", },
				{ "FontRender.frag.spv", },
				{ "FontRasterRender.vert.spv", },
				{ "FontRasterRender.frag.spv", },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
			}
		}

		// descriptor
		{
			auto stage = vk::ShaderStageFlagBits::eFragment;
			std::vector<vk::DescriptorSetLayoutBinding> binding =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setBinding(0),
			};
			m_descriptor_set_layout[1] = btr::Descriptor::createDescriptorSetLayout(context, binding);
//			m_descriptor_set = btr::Descriptor::allocateDescriptorSet(context, context->m_descriptor_pool.get(), m_descriptor_set_layout.get());

		}

		// setup pipeline_layout
		{
// 			vk::DescriptorSetLayout layouts[] = {
// //				m_descriptor_set_layout.get(),
// //				sSystem::Order().getSystemDescriptorLayout(),
// 			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
// 			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
// 			pipeline_layout_info.setPSetLayouts(layouts);
			// 		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			// 		pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				sSystem::Order().getSystemDescriptorLayout(),
				m_descriptor_set_layout[1].get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			// 		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			// 		pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PIPELINE_LAYOUT_RENDER_RASTER] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		FT_Init_FreeType(&m_library);

	}

	~sFont()
	{
		FT_Done_FreeType(m_library);
	}

};

struct CmdList
{
	void pushCmd(std::function<void()>&& cmd)
	{
		std::lock_guard<std::mutex> lg(m_cmd_mutex);
		m_cmd_list[sGlobal::Order().getWorkerIndex()].emplace_back(std::move(cmd));
	}
	std::vector<std::function<void()>>& getCmd()
	{
		return std::move(m_cmd_list[sGlobal::Order().getRenderIndex()]);
	}
	~CmdList()
	{
		assert(m_cmd_list[0].empty());
		assert(m_cmd_list[1].empty());
	}

	std::mutex m_cmd_mutex;
	std::array<std::vector<std::function<void()>>, 2> m_cmd_list;

};

struct PipelineDescription
{
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<RenderPassModule> m_render_pass;
};
struct FontRenderPipeline
{
	FontRenderPipeline(const std::shared_ptr<btr::Context>& context, const PipelineDescription& desc)
	{
		m_context = context;
		m_pipeline_description = desc;
		// setup pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eLineStrip),
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			auto& render_pass = m_pipeline_description.m_render_pass;
			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_pass->getResolution().width, (float)render_pass->getResolution().height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(render_pass->getResolution().width, render_pass->getResolution().height));
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount(1);
			viewportInfo.setPScissors(&scissor);

			vk::PipelineRasterizationStateCreateInfo rasterization_info[2];
			rasterization_info[0].setPolygonMode(vk::PolygonMode::eLine);
			rasterization_info[0].setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info[0].setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info[0].setLineWidth(1.f);
			rasterization_info[1].setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info[1].setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info[1].setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info[1].setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_TRUE)
				.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
				.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setAlphaBlendOp(vk::BlendOp::eAdd)
				.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
				.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::VertexInputAttributeDescription attr[] =
			{
				vk::VertexInputAttributeDescription().setLocation(0).setOffset(0).setBinding(0).setFormat(vk::Format::eR32G32Sint),
			};
			vk::VertexInputBindingDescription binding[] =
			{
				vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(8)
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info[2];
			vertex_input_info[0].setVertexAttributeDescriptionCount(array_length(attr));
			vertex_input_info[0].setPVertexAttributeDescriptions(attr);
			vertex_input_info[0].setVertexBindingDescriptionCount(array_length(binding));
			vertex_input_info[0].setPVertexBindingDescriptions(binding);

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				vk::PipelineShaderStageCreateInfo()
				.setModule(sFont::Order().getShaderModle(sFont::SHADER_VERT_RENDER))
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(sFont::Order().getShaderModle(sFont::SHADER_FRAG_RENDER))
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};
			vk::PipelineShaderStageCreateInfo shader_info_raster[] = {
				vk::PipelineShaderStageCreateInfo()
				.setModule(sFont::Order().getShaderModle(sFont::SHADER_RASTER_VERT_RENDER))
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(sFont::Order().getShaderModle(sFont::SHADER_RASTER_FRAG_RENDER))
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info[0])
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info[0])
				.setPMultisampleState(&sample_info)
				.setLayout(sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER))
				.setRenderPass(render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info_raster))
				.setPStages(shader_info_raster)
				.setPVertexInputState(&vertex_input_info[1])
				.setPInputAssemblyState(&assembly_info[1])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info[1])
				.setPMultisampleState(&sample_info)
				.setLayout(sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER_RASTER))
				.setRenderPass(render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[0] = std::move(pipelines[0]);
			m_pipeline[1] = std::move(pipelines[1]);

		}

		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR32Sfloat;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eLinear;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { 1, 1, 1 };
			//		image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
			auto image = context->m_device->createImageUnique(image_info);

			vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_gpu.getHandle(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			auto image_memory = context->m_device->allocateMemoryUnique(memory_alloc_info);
			context->m_device->bindImageMemory(image.get(), image_memory.get(), 0);

			btr::BufferMemoryDescriptor staging_desc;
			staging_desc.size = 4;
			staging_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_buffer = context->m_staging_memory.allocateMemory(staging_desc);
			*staging_buffer.getMappedPtr<float>() = 1.f;

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;

		}

		FT_New_Face(sFont::Order().m_library, (btr::getResourceAppPath() + "font\\" + "mgenplus-1c-black.ttf").c_str(), 0, &m_face);
		FT_Set_Pixel_Sizes(m_face, 16, 0);
		FT_Select_Charmap(m_face, FT_ENCODING_UNICODE);
		//		FT_Set_Char_Size(m_face, 16*64, 16*64, 640, 480);

		m_cache = new GlyphCache(context);
		m_descriptor_set = btr::Descriptor::allocateDescriptorSet(context, context->m_descriptor_pool.get(), sFont::Order().m_descriptor_set_layout[sFont::DESCRIPTOR_SET_LAYOUT_RASTER].get());

		vk::DescriptorImageInfo images[] = {
			vk::DescriptorImageInfo()
			.setImageView(m_cache->m_image_view_raster_cache.get())
			.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER))
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		};
		auto write_desc =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(array_length(images))
			.setPImageInfo(images)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set.get()),
		};
		context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});

	}
	~FontRenderPipeline()
	{
		FT_Done_Face(m_face);

	}


	void async(vk::CommandBuffer& cmd)
	{
#if 0
		// outline render
		auto& outline = m_face->glyph->outline;
		m_face->glyph->advance.x;
		auto index = FT_Get_Char_Index(m_face, 'a');
		auto error = FT_Load_Glyph(m_face, index, FT_LOAD_DEFAULT);

		vk::RenderPassBeginInfo begin_render_info;
		begin_render_info.setFramebuffer(m_pipeline_description.m_render_pass->getFramebuffer(m_pipeline_description.m_context->m_window->getSwapchain().m_backbuffer_index));
		begin_render_info.setRenderPass(m_pipeline_description.m_render_pass->getRenderPass());
		begin_render_info.setRenderArea(vk::Rect2D({}, m_pipeline_description.m_render_pass->getResolution()));
		cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());

		btr::BufferMemoryDescriptorEx<FT_Vector> v_desc;
		v_desc.element_num = outline.n_points;
		v_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto vertex = m_pipeline_description.m_context->m_staging_memory.allocateMemory(v_desc);
		memcpy(vertex.getMappedPtr(), outline.points, sizeof(FT_Vector)*outline.n_points);
		cmd.bindVertexBuffers(0, { vertex.getInfo().buffer }, { vertex.getInfo().offset });
		int voffset = 0;
		for (uint32_t i = 0; i < outline.n_contours; i++)
		{
			cmd.draw(outline.contours[i]-voffset, 1, voffset, 0);
			voffset = outline.contours[i]+1;
		}
		cmd.endRenderPass();
#endif

		auto& glyph = m_face->glyph;
		auto char_index = FT_Get_Char_Index(m_face, U'‚ ');

		GlyphCache::MetrixInfo* info = nullptr;
		for (uint32_t i = 0; i < m_cache->m_glyph_metrix.size(); i++)
		{
			if (m_cache->m_glyph_metrix[i].m_char_index == char_index)
			{
				info = &m_cache->m_glyph_metrix[i];
				break;
			}
		}

		if (!info)
		{
			// “o˜^
			for (uint32_t i = 0; i < m_cache->m_glyph_metrix.size(); i++)
			{
				if (m_cache->m_glyph_metrix[i].m_char_index == 0)
				{
					m_cache->m_glyph_metrix[i].m_cache_index = i;
					m_cache->m_glyph_metrix[i].m_char_index = char_index;
					auto error = FT_Load_Glyph(m_face, char_index, FT_LOAD_DEFAULT);
					//		FT_ERR(error);
					error = FT_Render_Glyph(m_face->glyph, FT_RENDER_MODE_NORMAL);
					//		FT_ERR(error);
					auto& bitmap = glyph->bitmap;
					info = &m_cache->m_glyph_metrix[i];

					btr::BufferMemoryDescriptorEx<unsigned char> v_desc;
					v_desc.element_num = bitmap.rows*bitmap.pitch;
					v_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
					auto staging = m_pipeline_description.m_context->m_staging_memory.allocateMemory(v_desc);
					memcpy(staging.getMappedPtr(), glyph->bitmap.buffer, staging.getDataSizeof() * v_desc.element_num);

					vk::BufferImageCopy copy;
					copy.bufferOffset = staging.getInfo().offset;
					copy.imageOffset = vk::Offset3D(i*16, 0, 0);
					copy.imageExtent = vk::Extent3D(bitmap.pitch, bitmap.rows, 1);
					copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
					copy.imageSubresource.baseArrayLayer = 0;
					copy.imageSubresource.layerCount = 1;
					copy.imageSubresource.mipLevel = 0;
					cmd.copyBufferToImage(staging.getInfo().buffer, m_cache->m_image_raster_cache.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
					{
						vk::ImageMemoryBarrier to_shader_read_barrier;
						to_shader_read_barrier.dstQueueFamilyIndex = m_context->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
						to_shader_read_barrier.image = m_cache->m_image_raster_cache.get();
						to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
						to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
						to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
						to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
						to_shader_read_barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
						to_shader_read_barrier.subresourceRange.baseArrayLayer = 0;
						to_shader_read_barrier.subresourceRange.baseMipLevel = 0;
						to_shader_read_barrier.subresourceRange.layerCount = 1;
						to_shader_read_barrier.subresourceRange.levelCount = 1;

						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
					}

					break;
				}
			}
		}
		vk::RenderPassBeginInfo begin_render_info;
		begin_render_info.setFramebuffer(m_pipeline_description.m_render_pass->getFramebuffer(m_pipeline_description.m_context->m_window->getSwapchain().m_backbuffer_index));
		begin_render_info.setRenderPass(m_pipeline_description.m_render_pass->getRenderPass());
		begin_render_info.setRenderArea(vk::Rect2D({}, m_pipeline_description.m_render_pass->getResolution()));
		cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[1].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER_RASTER), 0, { sSystem::Order().getSystemDescriptorSet() }, {0});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER_RASTER), 1, { m_descriptor_set.get() }, {});

		cmd.draw(4, 1, 0, 0);
		cmd.endRenderPass();
	}

	enum Pipeline
	{
		Pipeline_outline,
		Pipeline_raster,
		Pipeline_num,
	};
	std::array<vk::UniquePipeline, Pipeline_num> m_pipeline;
	CmdList m_cmd_list;
	PipelineDescription m_pipeline_description;

	FT_Face m_face;
	struct GlyphCache
	{
		GlyphCache(const std::shared_ptr<btr::Context>& context)
		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR8Unorm;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eLinear;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { 16*64, 16, 1 };
			m_image_raster_cache = context->m_device->createImageUnique(image_info);

			vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(m_image_raster_cache.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_gpu.getHandle(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			m_memory_raster_cache = context->m_device->allocateMemoryUnique(memory_alloc_info);
			context->m_device->bindImageMemory(m_image_raster_cache.get(), m_memory_raster_cache.get(), 0);

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eR;
			view_info.components.b = vk::ComponentSwizzle::eR;
			view_info.components.a = vk::ComponentSwizzle::eR;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = m_image_raster_cache.get();
			view_info.subresourceRange = subresourceRange;
			m_image_view_raster_cache = context->m_device->createImageViewUnique(view_info);

			vk::ImageMemoryBarrier init_barrier;
			init_barrier.image = m_image_raster_cache.get();
			init_barrier.oldLayout = vk::ImageLayout::eUndefined;
			init_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
			init_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			init_barrier.subresourceRange = subresourceRange;
			auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { init_barrier });


		}

		struct MetrixInfo
		{
			// https://www.freetype.org/freetype2/docs/tutorial/step2.html
			uint32_t m_char_index;
			uint32_t m_cache_index;
			MetrixInfo()
			{
				m_cache_index = 0;
				m_char_index = 0;
			}
		};
		std::array<MetrixInfo, 64> m_glyph_metrix;
		vk::UniqueImage m_image_raster_cache;
		vk::UniqueImageView m_image_view_raster_cache;
		vk::UniqueDeviceMemory m_memory_raster_cache;

// 		MetrixInfo* tryGlyph()
// 		{
// 
// 		}
	};

	GlyphCache* m_cache;
	vk::UniqueDescriptorSet	m_descriptor_set;
	std::shared_ptr<btr::Context> m_context;
};

