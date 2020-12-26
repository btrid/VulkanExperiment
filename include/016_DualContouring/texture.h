#pragma once

#include <png.h>
#pragma comment(lib, "libpng16_static.lib")
#pragma comment(lib, "zlibstatic.lib")


struct TextureResource
{
	vk::UniqueImage m_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_memory;
	vk::UniqueSampler m_sampler;

	static TextureResource read_png_file(btr::Context& ctx, const char* file_name)
	{
		FILE* fp;
		fopen_s(&fp, file_name, "rb");
		assert(fp);

		png_byte sig[4];
		if (fread(sig, 4, 1, fp) < 1)
		{
			fclose(fp);
			assert(false);
		}
		if (!png_check_sig(sig, 4))
		{
			fclose(fp);
			assert(false);
		}

		png_structp Png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		png_infop PngInfo = png_create_info_struct(Png);

		int offset = 8;
		png_init_io(Png, fp);
		png_set_sig_bytes(Png, 4);
		png_read_info(Png, PngInfo);
		//	png_read_png(Png, PngInfo, PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_EXPAND, NULL);


		int c = png_get_channels(Png, PngInfo);
		uint32_t w, h;
		int d, ct, interlacetype;
		png_get_IHDR(Png, PngInfo, &w, &h, &d, &ct, &interlacetype, NULL, NULL);

		switch (ct)
		{
		case PNG_COLOR_TYPE_PALETTE:
			png_set_palette_to_rgb(Png);
			//		type = vk::Format::;
			break;

		case PNG_COLOR_TYPE_RGB:
			//		type = GL_RGB;
			break;

		case PNG_COLOR_TYPE_RGBA:
			//		gl_type = GL_RGBA;
			break;

		default:
			// 対応していないタイプなので処理を終了
			assert(false);
			return {};
		}

		uint32_t row_bytes = png_get_rowbytes(Png, PngInfo);
		std::vector<byte> buf(row_bytes * h);
		for (int i = 0; i < h; i++)
		{
			png_read_row(Png, buf.data() + i * row_bytes, NULL);
		}

		png_read_end(Png, NULL);
		png_destroy_read_struct(&Png, &PngInfo, (png_infopp)NULL);
		fclose(fp);

		TextureResource texture;
		{
			vk::Format formats[4][4] =
			{
				{vk::Format::eR8Unorm, vk::Format::eR16Unorm, vk::Format::eR32Sfloat, vk::Format::eR32Sfloat},
				{vk::Format::eR8G8Unorm, vk::Format::eR16G16Unorm, vk::Format::eR32G32Sfloat, vk::Format::eR32G32Sfloat},
				{vk::Format::eR8G8B8Unorm, vk::Format::eR16G16B16Unorm, vk::Format::eR32G32B32Sfloat, vk::Format::eR32G32B32Sfloat},
				{vk::Format::eR8G8B8A8Unorm, vk::Format::eR16G16B16A16Unorm, vk::Format::eR32G32B32A32Sfloat, vk::Format::eR32G32B32A32Sfloat},
			};
			d = d / 8;
			vk::Format format = formats[c - 1][d - 1];

			auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = format;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eLinear;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = vk::Extent3D{ w, h, 1 };
			//		image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
			vk::UniqueImage image = ctx.m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			vk::UniqueDeviceMemory image_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
			ctx.m_device.bindImageMemory(image.get(), image_memory.get(), 0);

			btr::BufferMemoryDescriptor staging_desc;
			auto staging_buffer = ctx.m_staging_memory.allocateMemory(buf.size(), true);
			memcpy(staging_buffer.getMappedPtr<char*>(), buf.data(), buf.size());

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = 1;

			{
				// staging_bufferからimageへコピー

				vk::BufferImageCopy copy;
				copy.bufferOffset = staging_buffer.getInfo().offset;
				copy.imageExtent = vk::Extent3D{ w, h, 1 };
				copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				copy.imageSubresource.baseArrayLayer = 0;
				copy.imageSubresource.layerCount = 1;
				copy.imageSubresource.mipLevel = 0;

				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = subresourceRange;

				vk::ImageMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.image = image.get();
				to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read_barrier.subresourceRange = subresourceRange;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				cmd.copyBufferToImage(staging_buffer.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = format;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eNearest;
			sampler_info.minFilter = vk::Filter::eNearest;
			sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
			sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
			sampler_info.mipLodBias = 0.0f;
			sampler_info.compareOp = vk::CompareOp::eNever;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = 0.f;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

			texture.m_image = std::move(image);
			texture.m_memory = std::move(image_memory);
			texture.m_image_view = ctx.m_device.createImageViewUnique(view_info);
			texture.m_sampler = ctx.m_device.createSamplerUnique(sampler_info);

		}

		return texture;

	}

};
