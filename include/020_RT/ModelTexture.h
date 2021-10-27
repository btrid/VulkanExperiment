#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <future>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/sGlobal.h>
#include <applib/GraphicsResource.h>

struct ModelTexture
{
	std::string m_filename;
	uint32_t m_block;

	vk::UniqueImage m_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_memory;
	vk::UniqueSampler m_sampler;

	vk::DescriptorImageInfo info()
	{
		return vk::DescriptorImageInfo().setSampler(m_sampler.get()).setImageView(m_image_view.get()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
	}

};

struct ModelResource
{
	std::unordered_map<std::string, std::weak_ptr<ModelTexture>> m_resource_list;
	std::array<uint32_t, 1024> m_active;
	uint32_t m_consume;
	uint32_t m_accume;
	std::mutex m_mutex;

	std::array<vk::DescriptorImageInfo, 1024> m_image_info;
	vk::UniqueDescriptorPool m_descriptor_pool;
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS_ModelTexture;
	vk::UniqueDescriptorUpdateTemplate m_Texture_DUP;
	ModelResource(btr::Context& ctx)
	{
		m_consume = 0;
		m_accume = 0;
		for (size_t i = 0; i < m_active.size(); i++)
		{
			m_active[i] = i;
		}

		{
			std::array<vk::DescriptorPoolSize, 1> pool_size;
			pool_size[0].setType(vk::DescriptorType::eCombinedImageSampler);
			pool_size[0].setDescriptorCount(m_active.size());

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount(array_length(pool_size));
			pool_info.setPPoolSizes(pool_size.data());
			pool_info.setMaxSets(1);
			pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
			m_descriptor_pool = ctx.m_device.createDescriptorPoolUnique(pool_info);

		}

		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1024, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		m_image_info.fill(vk::DescriptorImageInfo{ sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal });
		{
			vk::DescriptorUpdateTemplateEntry dutEntry[1];
			dutEntry[0].setDstBinding(0).setDstArrayElement(0).setDescriptorCount(array_size(m_image_info)).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setOffset(offsetof(ModelResource, m_image_info)).setStride(sizeof(VkDescriptorImageInfo));
		
			vk::DescriptorUpdateTemplateCreateInfo dutCI;
			dutCI.setTemplateType(vk::DescriptorUpdateTemplateType::eDescriptorSet);
			dutCI.descriptorSetLayout = m_DSL.get();
			dutCI.descriptorUpdateEntryCount = array_size(dutEntry);
			dutCI.pDescriptorUpdateEntries = dutEntry;
			m_Texture_DUP = ctx.m_device.createDescriptorUpdateTemplateUnique(dutCI);
		}
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS_ModelTexture = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			ctx.m_device.updateDescriptorSetWithTemplate(*m_DS_ModelTexture, *m_Texture_DUP, this);
		}
	}
	/**
	 * @brief	リソースの管理を行う
	 *			@return true	すでに管理されている
	 *					false	新しいリソースを生成した
	 */
	std::shared_ptr<ModelTexture> make(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename, const vk::ImageCreateInfo& info, const std::vector<byte>& data)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_resource_list.find(filename);
		if (it != m_resource_list.end())
		{
			return it->second.lock();
		}
		auto deleter = [&](ModelTexture* ptr) { release(ptr); sDeleter::Order().enque(std::unique_ptr<ModelTexture>(ptr)); };
		auto resource = std::shared_ptr<ModelTexture>(new ModelTexture, deleter);
		resource->m_filename = filename;
		resource->m_block = m_active[m_consume];
		{
			vk::UniqueImage image = ctx.m_device.createImageUnique(info);

			vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			vk::UniqueDeviceMemory image_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
			ctx.m_device.bindImageMemory(image.get(), image_memory.get(), 0);

			auto staging_buffer = ctx.m_staging_memory.allocateMemory<byte>(data.size(), true);
			memcpy(staging_buffer.getMappedPtr(), data.data(), data.size());

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
				copy.imageExtent = info.extent;
				copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
				copy.imageSubresource.baseArrayLayer = 0;
				copy.imageSubresource.layerCount = 1;
				copy.imageSubresource.mipLevel = 0;

				{
					vk::ImageMemoryBarrier to_copy_barrier;
					to_copy_barrier.image = image.get();
					to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
					to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
					to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_copy_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				}
				cmd.copyBufferToImage(staging_buffer.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
				{
					vk::ImageMemoryBarrier to_shader_read_barrier;
					to_shader_read_barrier.image = image.get();
					to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
					to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
					to_shader_read_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
				}

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e2D;
			view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = info.format;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eLinear;
			sampler_info.minFilter = vk::Filter::eLinear;
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

			resource->m_image = std::move(image);
			resource->m_memory = std::move(image_memory);
			resource->m_image_view = ctx.m_device.createImageViewUnique(view_info);
			resource->m_sampler = ctx.m_device.createSamplerUnique(sampler_info);
		}

		m_image_info[resource->m_block] = resource->info();
		ctx.m_device.updateDescriptorSetWithTemplate(*m_DS_ModelTexture, *m_Texture_DUP, this);

		m_resource_list[filename] = resource;
		m_consume = (m_consume + 1) % m_active.size();
		return resource;
	}

private:

	void release(ModelTexture* p)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_resource_list.erase(p->m_filename);
		m_active[m_accume] = p->m_block;
		m_accume = (m_accume + 1) % m_active.size();
	}
};
