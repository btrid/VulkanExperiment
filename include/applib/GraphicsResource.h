#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/Singleton.h>
struct sGraphicsResource : public Singleton<sGraphicsResource>
{
	void setup(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

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
		image_info.extent = vk::Extent3D{ 1, 1, 1 };
//		image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
		auto image = context->m_device.createImageUnique(image_info);

		vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(image.get());
		vk::MemoryAllocateInfo memory_alloc_info;
		memory_alloc_info.allocationSize = memory_request.size;
		memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		auto image_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
		context->m_device.bindImageMemory(image.get(), image_memory.get(), 0);

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

		{
			// staging_bufferからimageへコピー

			vk::BufferImageCopy copy;
			copy.bufferOffset = staging_buffer.getInfo().offset;
			copy.imageExtent = vk::Extent3D{ 1, 1, 1 };
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
		view_info.components.g = vk::ComponentSwizzle::eR;
		view_info.components.b = vk::ComponentSwizzle::eR;
		view_info.components.a = vk::ComponentSwizzle::eR;
		view_info.flags = vk::ImageViewCreateFlags();
		view_info.format = vk::Format::eR32Sfloat;
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

		m_whilte_texture.m_image = std::move(image);
		m_whilte_texture.m_memory = std::move(image_memory);
		m_whilte_texture.m_image_view = context->m_device.createImageViewUnique(view_info);
		m_whilte_texture.m_sampler = context->m_device.createSamplerUnique(sampler_info);


		vk::SamplerCreateInfo sampler_infos[2];
		static_assert(array_length(sampler_infos) == BASIC_SAMPLER_NUM, "");
		sampler_infos[0].magFilter = vk::Filter::eNearest;
		sampler_infos[0].minFilter = vk::Filter::eNearest;
		sampler_infos[0].mipmapMode = vk::SamplerMipmapMode::eNearest;
		sampler_infos[0].addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_infos[0].addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_infos[0].addressModeW = vk::SamplerAddressMode::eClampToEdge;
		sampler_infos[0].mipLodBias = 0.0f;
		sampler_infos[0].compareOp = vk::CompareOp::eNever;
		sampler_infos[0].minLod = 0.0f;
		sampler_infos[0].maxLod = 0.f;
		sampler_infos[0].maxAnisotropy = 1.0;
		sampler_infos[0].anisotropyEnable = VK_FALSE;
		sampler_infos[0].borderColor = vk::BorderColor::eFloatOpaqueWhite;
		sampler_infos[1].magFilter = vk::Filter::eLinear;
		sampler_infos[1].minFilter = vk::Filter::eLinear;
		sampler_infos[1].mipmapMode = vk::SamplerMipmapMode::eLinear;
		sampler_infos[1].addressModeU = vk::SamplerAddressMode::eClampToEdge;
		sampler_infos[1].addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_infos[1].addressModeW = vk::SamplerAddressMode::eClampToEdge;
		sampler_infos[1].mipLodBias = 0.0f;
		sampler_infos[1].compareOp = vk::CompareOp::eNever;
		sampler_infos[1].minLod = 0.0f;
		sampler_infos[1].maxLod = 0.f;
		sampler_infos[1].maxAnisotropy = 1.0;
		sampler_infos[1].anisotropyEnable = VK_FALSE;
		sampler_infos[1].borderColor = vk::BorderColor::eFloatOpaqueWhite;
		for (uint32_t i = 0; i < BASIC_SAMPLER_NUM; i++)
		{
			m_basic_samplers[i] = context->m_device.createSamplerUnique(sampler_infos[i]);
		}
	}

	struct TextureResource
	{
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;
	};
	TextureResource m_whilte_texture; //!< ダミーテクスチャ
	TextureResource& getWhiteTexture() { return m_whilte_texture; }

	enum BasicTexture
	{
		BASIC_TEXTURE_WHITE,
		BASIC_TEXTURE_BLACK,
		BASIC_TEXTURE_NUM,
	};
	enum BasicSampler
	{
		BASIC_SAMPLER_POINT,
		BASIC_SAMPLER_LINER,
		BASIC_SAMPLER_NUM,
	};
	vk::UniqueImageView m_image_view;
	std::array<vk::UniqueImageView, BASIC_TEXTURE_NUM> m_basic_textures;
	std::array<vk::UniqueSampler, BASIC_SAMPLER_NUM> m_basic_samplers;
	vk::ImageView getImageView(BasicTexture index) { return m_basic_textures[index].get(); }
	vk::Sampler getSampler(BasicSampler index) { return m_basic_samplers[index].get(); }

};