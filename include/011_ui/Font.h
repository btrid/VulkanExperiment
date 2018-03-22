#pragma once

#include <string>
#include <ft2build.h>
#include <freetype/freetype.h>
//#include <freetype/fterrdef.h>

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <applib/GraphicsResource.h>
#include <applib/sSystem.h>

// #undef FTERRORS_H_
// #define FT_ERRORDEF( e, v, s )  { e, s },
// #define FT_ERROR_START_LIST     {
// #define FT_ERROR_END_LIST       { 0, NULL } };
// const struct
// {
// 	int          err_code;
// 	const char*  err_msg;
// } ft_errors[] =
// #include FT_ERRORS_H

#define assert_ft(_error) { if (_error != FT_Err_Ok) { assert(false);} }

struct GlyphInfo
{
	uint32_t m_char_index;
	uint32_t m_cache_index;
	ivec2 m_offset;
	uvec2 m_size;
	ivec2 m_begin;
	GlyphInfo()
	{
		m_cache_index = 0;
		m_char_index = 0;
	}
};

struct TextRequest
{
	std::u32string m_text;
	uvec2 m_area_pos;		//!< 左上
	uvec2 m_area_size;		//!< 大きさ;
	bool m_vertical;		//!< 縦書き？

	TextRequest()
		: m_vertical(false)
	{
	}
};

struct TextData
{
	std::vector<GlyphInfo> m_info;
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


	vk::UniqueDescriptorSetLayout m_descriptor_set_layout_glyph;
	vk::UniqueDescriptorSet m_descriptor_set_glyph;
	btr::BufferMemoryEx<GlyphInfo> m_glyph_map_buffer;
	sFont(const std::shared_ptr<btr::Context>& context)
	{
		{
			{
				btr::BufferMemoryDescriptorEx<GlyphInfo> desc;
				desc.element_num = 256;
				m_glyph_map_buffer = context->m_storage_memory.allocateMemory(desc);
			}

			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
			std::vector<vk::DescriptorSetLayoutBinding> binding =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
			};
			m_descriptor_set_layout_glyph = btr::Descriptor::createDescriptorSetLayout(context, binding);
			m_descriptor_set_glyph = btr::Descriptor::allocateDescriptorSet(context, context->m_descriptor_pool.get(), m_descriptor_set_layout_glyph.get());

			vk::DescriptorBufferInfo storages[] = {
				m_glyph_map_buffer.getInfo(),
			};
			auto write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set_glyph.get()),
			};
			context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});
		}

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
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
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
				m_descriptor_set_layout_glyph.get(),
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

struct GlyphCacheDescription
{
	uint32_t m_glyph_num;
};
struct GlyphCache
{
	struct CacheInfo
	{
		uvec2 m_glyph_size;
		uvec2 m_glyph_tex_size;
	};

	GlyphCacheDescription m_description;
	std::vector<GlyphInfo> m_glyph_info;
	vk::UniqueImage m_image_raster_cache;
	vk::UniqueImageView m_image_view_raster_cache;
	vk::UniqueDeviceMemory m_memory_raster_cache;
	vk::UniqueDescriptorSet	m_descriptor_set;

	btr::BufferMemoryEx<CacheInfo> m_cache_info;
};

struct PipelineDescription
{
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<RenderTarget> m_render_target;
};

struct FontDescription
{
	std::string filename;
	uvec2 m_glyph_size;
};
struct Font : std::enable_shared_from_this<Font>
{
	Font(const FontDescription& font_desc)
	{
		m_description = font_desc;

		FT_New_Face(sFont::Order().m_library, (btr::getResourceAppPath() + "font\\" + font_desc.filename).c_str(), 0, &m_face);
		FT_Set_Pixel_Sizes(m_face, font_desc.m_glyph_size.x, font_desc.m_glyph_size.y);
		FT_Select_Charmap(m_face, FT_ENCODING_UNICODE);
	}

	~Font()
	{
		FT_Done_Face(m_face);
	}

	std::unique_ptr<GlyphCache> makeCache(const std::shared_ptr<btr::Context> context, const GlyphCacheDescription& cache_desc)
	{
		std::unique_ptr<GlyphCache> cache = std::make_unique<GlyphCache>();
		vk::ImageCreateInfo image_info;
		image_info.imageType = vk::ImageType::e2D;
		image_info.format = vk::Format::eR8Unorm;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = vk::SampleCountFlagBits::e1;
		image_info.tiling = vk::ImageTiling::eOptimal;
		image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
		image_info.sharingMode = vk::SharingMode::eExclusive;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
		image_info.extent.width = (m_description.m_glyph_size * cache_desc.m_glyph_num).x;
		image_info.extent.height = (m_description.m_glyph_size * 1).y;
		image_info.extent.depth = 1;
		cache->m_image_raster_cache = context->m_device->createImageUnique(image_info);

		vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(cache->m_image_raster_cache.get());
		vk::MemoryAllocateInfo memory_alloc_info;
		memory_alloc_info.allocationSize = memory_request.size;
		memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_gpu.getHandle(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		cache->m_memory_raster_cache = context->m_device->allocateMemoryUnique(memory_alloc_info);
		context->m_device->bindImageMemory(cache->m_image_raster_cache.get(), cache->m_memory_raster_cache.get(), 0);

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
		view_info.image = cache->m_image_raster_cache.get();
		view_info.subresourceRange = subresourceRange;
		cache->m_image_view_raster_cache = context->m_device->createImageViewUnique(view_info);

		vk::ImageMemoryBarrier init_barrier;
		init_barrier.image = cache->m_image_raster_cache.get();
		init_barrier.oldLayout = vk::ImageLayout::eUndefined;
		init_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
		init_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
		init_barrier.subresourceRange = subresourceRange;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { init_barrier });

		{
			btr::BufferMemoryDescriptorEx<GlyphCache::CacheInfo> desc;
			desc.element_num = 1;
			cache->m_cache_info = context->m_uniform_memory.allocateMemory(desc);

			GlyphCache::CacheInfo info;
			info.m_glyph_size = m_description.m_glyph_size;
			info.m_glyph_tex_size = m_description.m_glyph_size * cache_desc.m_glyph_num;
			cmd.updateBuffer<GlyphCache::CacheInfo>(cache->m_cache_info.getInfo().buffer, cache->m_cache_info.getInfo().offset, info);

			cache->m_glyph_info.resize(m_description.m_glyph_size.x*m_description.m_glyph_size.y);
		}
		{
			cache->m_descriptor_set = btr::Descriptor::allocateDescriptorSet(context, context->m_descriptor_pool.get(), sFont::Order().m_descriptor_set_layout[sFont::DESCRIPTOR_SET_LAYOUT_RASTER].get());

			vk::DescriptorImageInfo images[] = {
				vk::DescriptorImageInfo()
				.setImageView(cache->m_image_view_raster_cache.get())
				.setSampler(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_POINT))
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
			};
			auto write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(array_length(images))
				.setPImageInfo(images)
				.setDstBinding(0)
				.setDstSet(cache->m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(write_desc.size(), write_desc.begin(), 0, {});
		}
		return cache;
	}

	std::unique_ptr<TextData> makeRender(const std::shared_ptr<btr::Context> context, const TextRequest& request, std::unique_ptr<GlyphCache>& cache)
	{
		
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		auto& glyph = m_face->glyph;
		std::unique_ptr<TextData> data = std::make_unique<TextData>();
		std::vector<GlyphInfo> infos(request.m_text.size());

		uvec2 offset;
		for (uint32_t i = 0; i < infos.size(); i++)
		{
			const auto& t = request.m_text[i];
			auto char_index = FT_Get_Char_Index(m_face, t);

			GlyphInfo* info = nullptr;
			for (uint32_t n = 0; n < cache->m_glyph_info.size(); n++)
			{
				if (cache->m_glyph_info[n].m_char_index == char_index)
				{
					info = &cache->m_glyph_info[n];
					break;
				}
			}

			if (!info)
			{
				// 登録
				// @todo copyを複数回行っているので無駄が多い
				for (uint32_t n = 0; n < cache->m_glyph_info.size(); n++)
				{
					if (cache->m_glyph_info[n].m_char_index == 0)
					{
						info = &cache->m_glyph_info[n];
						info->m_cache_index = n;
						info->m_char_index = char_index;

						auto error = FT_Load_Glyph(m_face, char_index, FT_LOAD_NO_HINTING /*| FT_LOAD_VERTICAL_LAYOUT*/);
						assert_ft(error);
						error = FT_Render_Glyph(glyph, FT_RENDER_MODE_NORMAL);
						assert_ft(error);
						auto& bitmap = glyph->bitmap;
						info->m_size = uvec2(bitmap.width, bitmap.rows);
						info->m_offset = uvec2(glyph->bitmap_left, glyph->bitmap_top);

						btr::BufferMemoryDescriptorEx<unsigned char> v_desc;
						v_desc.element_num = bitmap.rows*bitmap.width;
						v_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
						auto staging = context->m_staging_memory.allocateMemory(v_desc);
						memcpy(staging.getMappedPtr(), glyph->bitmap.buffer, staging.getDataSizeof() * v_desc.element_num);

						vk::BufferImageCopy copy;
						copy.bufferOffset = staging.getInfo().offset;
						copy.imageOffset = vk::Offset3D(n * m_description.m_glyph_size.x, 0, 0);
						copy.imageExtent = vk::Extent3D(bitmap.width, bitmap.rows, 1);
						copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						copy.imageSubresource.baseArrayLayer = 0;
						copy.imageSubresource.layerCount = 1;
						copy.imageSubresource.mipLevel = 0;

						cmd.copyBufferToImage(staging.getInfo().buffer, cache->m_image_raster_cache.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
						break;
					}
				}
			}
			uvec2 advance;
			if (!request.m_vertical)
			{
				advance.x = glyph->metrics.horiAdvance / 64;
				if (offset.x + info->m_offset.x >= request.m_area_size.x)
				{
					// 改行
					offset.x = request.m_area_pos.x;
					offset.y += m_description.m_glyph_size.y;
				}
			}
			else
			{
				advance.y = glyph->metrics.vertAdvance / 64;
				if (offset.y - info->m_offset.y >= request.m_area_size.y)
				{
					// 改行
					offset.x -= m_description.m_glyph_size.x;
					offset.y = request.m_area_pos.y;
				}
			}

			// 文字を配置
			infos[i] = *info;
			infos[i].m_begin = offset;
			infos[i].m_begin += uvec2(info->m_offset.x, -info->m_offset.y);
			offset += advance;
		}

		{
			vk::ImageMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.dstQueueFamilyIndex = context->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.image = cache->m_image_raster_cache.get();
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
		data->m_info = std::move(infos);
		return data;
	}

	FontDescription m_description;
	FT_Face m_face;
};
struct FontRenderer
{
	FontRenderer(const std::shared_ptr<btr::Context>& context, const PipelineDescription& desc)
	{
		m_context = context;
		m_pipeline_description = desc;

		const auto& render_target = m_pipeline_description.m_render_target;

		// レンダーパス
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

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_target->m_resolution.width, (float)render_target->m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(render_target->m_resolution.width, render_target->m_resolution.height));
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
				.setRenderPass(m_render_pass.get())
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
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[0] = std::move(pipelines[0]);
			m_pipeline[1] = std::move(pipelines[1]);

		}

	}

	void draw(vk::CommandBuffer& cmd, const Font& font, std::unique_ptr<GlyphCache>& cache, const std::unique_ptr<TextData>& data)
	{
		auto& glyph = font.m_face->glyph;

		{
			btr::BufferMemoryDescriptorEx<GlyphInfo> desc;
			desc.element_num = data->m_info.size();
			auto staging = m_context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), vector_sizeof(data->m_info), data->m_info.data(), vector_sizeof(data->m_info));

			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(sFont::Order().m_glyph_map_buffer.getInfo().offset);
			copy.setSize(vector_sizeof(data->m_info));
			cmd.copyBuffer(staging.getInfo().buffer, sFont::Order().m_glyph_map_buffer.getInfo().buffer, { copy });
		}


		vk::RenderPassBeginInfo begin_render_info;
		begin_render_info.setFramebuffer(m_framebuffer.get());
		begin_render_info.setRenderPass(m_render_pass.get());
		begin_render_info.setRenderArea(vk::Rect2D({}, m_pipeline_description.m_render_target->m_resolution));
		cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[1].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER_RASTER), 0, { sSystem::Order().getSystemDescriptorSet() }, { 0 });
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER_RASTER), 1, { cache->m_descriptor_set.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER_RASTER), 2, { sFont::Order().m_descriptor_set_glyph.get() }, {});


		cmd.draw(4, data->m_info.size(), 0, 0);
		cmd.endRenderPass();

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

	}

	enum Pipeline
	{
		Pipeline_outline,
		Pipeline_raster,
		Pipeline_num,
	};
	std::array<vk::UniquePipeline, Pipeline_num> m_pipeline;
	PipelineDescription m_pipeline_description;
	std::shared_ptr<btr::Context> m_context;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

};

