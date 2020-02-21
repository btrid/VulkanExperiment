#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <btrlib/Singleton.h>
#include <btrlib/cWindow.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/Geometry.h>

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "imgui.lib")


struct Sky 
{
	enum Shader
	{
		Shader_SkyReference_CS,
		Shader_SkyAriseReference_CS,

		Shader_Sky_Render_CS,
		Shader_Sky_CalcDensity_CS,
		Shader_Sky_MakeTexture_PartialX_CS,
		Shader_Sky_MakeTexture_PartialY_CS,
		Shader_Sky_MakeTexture_PartialZ_CS,

		Shader_SkyArise_CS,
		Shader_SkyArise_MakeTexture_CS,

		Shader_Sky_RenderUpsampling_CS,
		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Sky_CS,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_SkyReference_CS,
		Pipeline_SkyAriseReference_CS,
		Pipeline_Sky_Render_CS,
		Pipeline_Sky_CalcDensity_CS,
		Pipeline_Sky_MakeTexture_PartialX_CS,
		Pipeline_Sky_MakeTexture_PartialY_CS,
		Pipeline_Sky_MakeTexture_PartialZ_CS,

		Pipeline_SkyArise_CS,
		Pipeline_SkyArise_MakeTexture_CS,

		Pipeline_Sky_RenderUpsampling_CS,
		Pipeline_Num,
	};

	std::shared_ptr<btr::Context> m_context;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	btr::BufferMemoryEx<uint8_t> u_hash;
	btr::BufferMemoryEx<uvec4> b_density;

	vk::ImageCreateInfo m_image_density_info;
	vk::UniqueImage m_image_density;
	vk::UniqueImageView m_image_density_view;
	vk::UniqueImageView m_image_density_write_view;
	vk::UniqueDeviceMemory m_image_density_memory;
	vk::UniqueSampler m_image_density_sampler;

	vk::ImageCreateInfo m_image_arise_info;
	vk::UniqueImage m_image_arise;
	vk::UniqueImageView m_image_arise_view;
	vk::UniqueImageView m_image_arise_write_view;
	vk::UniqueDeviceMemory m_image_arise_memory;
	vk::UniqueSampler m_image_arise_sampler;

	vk::ImageCreateInfo m_image_shadow_info;
	vk::UniqueImage m_image_shadow;
	vk::UniqueImageView m_image_shadow_view;
	vk::UniqueImageView m_image_shadow_write_view;
	vk::UniqueDeviceMemory m_image_shadow_memory;
	vk::UniqueSampler m_image_shadow_sampler;

	vk::ImageCreateInfo m_image_render_info;
	vk::UniqueImage m_image_render;
	vk::UniqueImageView m_image_render_view;
	vk::UniqueImageView m_image_render_write_view;
	vk::UniqueDeviceMemory m_image_render_memory;
	vk::UniqueSampler m_image_render_sampler;

	Sky(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = context;
		// descriptor layout
		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, stage),
				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageImage, 1, stage),
				vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eStorageImage, 1, stage),
				vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eStorageImage, 1, stage),
				vk::DescriptorSetLayoutBinding(13, vk::DescriptorType::eStorageImage, 1, stage),
				vk::DescriptorSetLayoutBinding(20, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(30, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}
		// descriptor set
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};

				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			}
			{
				u_hash = context->m_uniform_memory.allocateMemory<uint8_t>(64);
				b_density = context->m_storage_memory.allocateMemory<uvec4>(512 * 16 * 512 / 16);

				std::vector<uint8_t> hash(64);
				for (auto& h : hash)
				{
					h = rand() % 32;
				}
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.updateBuffer<uint8_t>(u_hash.getInfo().buffer, u_hash.getInfo().offset, hash);
			}

			{
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(256, 16 * 1, 256));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR8Unorm);
				image_info.setImageType(vk::ImageType::e3D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_density = context->m_device.createImageUnique(image_info);
				m_image_density_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_density.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_density_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_density.get(), m_image_density_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_density.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e3D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eIdentity).setB(vk::ComponentSwizzle::eIdentity).setA(vk::ComponentSwizzle::eIdentity);
				m_image_density_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8Uint);
				m_image_density_write_view = context->m_device.createImageViewUnique(view_info);

				vk::SamplerCreateInfo sampler_info;
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eRepeat);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eRepeat);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eRepeat);
				sampler_info.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
				sampler_info.setMagFilter(vk::Filter::eLinear);
				sampler_info.setMinFilter(vk::Filter::eLinear);
				sampler_info.setMinLod(0.f);
				sampler_info.setMaxLod(0.f);
				sampler_info.setMipLodBias(0.f);
				sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
				sampler_info.setUnnormalizedCoordinates(false);
				m_image_density_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_density.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

			}

			{
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(m_image_density_info.extent.width/4, m_image_density_info.extent.height / 4, m_image_density_info.extent.depth / 4));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR8Unorm);
				image_info.setImageType(vk::ImageType::e3D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_shadow = context->m_device.createImageUnique(image_info);
				m_image_shadow_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_shadow.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_shadow_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_shadow.get(), m_image_shadow_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_shadow.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e3D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eIdentity).setB(vk::ComponentSwizzle::eIdentity).setA(vk::ComponentSwizzle::eIdentity);
				m_image_shadow_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8Uint);
				m_image_shadow_write_view = context->m_device.createImageViewUnique(view_info);

				vk::SamplerCreateInfo sampler_info;
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eRepeat);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eRepeat);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eRepeat);
				sampler_info.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
				sampler_info.setMagFilter(vk::Filter::eLinear);
				sampler_info.setMinFilter(vk::Filter::eLinear);
				sampler_info.setMinLod(0.f);
				sampler_info.setMaxLod(0.f);
				sampler_info.setMipLodBias(0.f);
				sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
				sampler_info.setUnnormalizedCoordinates(false);
				m_image_shadow_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_shadow.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

			}
			{
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(render_target->m_info.extent.width/2, render_target->m_info.extent.height/2, 1));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR16G16B16A16Sfloat);
				image_info.setImageType(vk::ImageType::e2D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_render = context->m_device.createImageUnique(image_info);
				m_image_render_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_render.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_render_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_render.get(), m_image_render_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_render.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e2D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eIdentity).setB(vk::ComponentSwizzle::eIdentity).setA(vk::ComponentSwizzle::eIdentity);
				m_image_render_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR16G16B16A16Sfloat);
				m_image_render_write_view = context->m_device.createImageViewUnique(view_info);

				vk::SamplerCreateInfo sampler_info;
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eRepeat);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eRepeat);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eRepeat);
				sampler_info.setMagFilter(vk::Filter::eLinear);
				sampler_info.setMinFilter(vk::Filter::eLinear);
				sampler_info.setMinLod(0.f);
				sampler_info.setMaxLod(0.f);
				sampler_info.setMipLodBias(0.f);
				sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
				sampler_info.setUnnormalizedCoordinates(false);
				m_image_render_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_render.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

			}
			{
				vk::ImageCreateInfo image_info;
				//				image_info.setExtent(vk::Extent3D(256, 256, 1));
				image_info.setExtent(vk::Extent3D(1024, 1024, 1));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR8G8B8A8Unorm);
				image_info.setImageType(vk::ImageType::e2D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_arise = context->m_device.createImageUnique(image_info);
				m_image_arise_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_arise.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_arise_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_arise.get(), m_image_arise_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_arise.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e2D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eG).setB(vk::ComponentSwizzle::eB).setA(vk::ComponentSwizzle::eA);
				m_image_arise_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8G8B8A8Uint);
				m_image_arise_write_view = context->m_device.createImageViewUnique(view_info);

				vk::SamplerCreateInfo sampler_info;
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
				sampler_info.setMagFilter(vk::Filter::eLinear);
				sampler_info.setMinFilter(vk::Filter::eLinear);
				sampler_info.setMinLod(0.f);
				sampler_info.setMaxLod(0.f);
				sampler_info.setMipLodBias(0.f);
				sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
				sampler_info.setUnnormalizedCoordinates(false);
				m_image_arise_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_arise.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

			}
			{
				vk::ImageCreateInfo image_info;
				//				image_info.setExtent(vk::Extent3D(256, 256, 1));
				image_info.setExtent(vk::Extent3D(1024, 1024, 1));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR8G8B8A8Unorm);
				image_info.setImageType(vk::ImageType::e2D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_arise = context->m_device.createImageUnique(image_info);
				m_image_arise_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_arise.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_arise_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_arise.get(), m_image_arise_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_arise.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e2D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eG).setB(vk::ComponentSwizzle::eB).setA(vk::ComponentSwizzle::eA);
				m_image_arise_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8G8B8A8Uint);
				m_image_arise_write_view = context->m_device.createImageViewUnique(view_info);

				vk::SamplerCreateInfo sampler_info;
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eClampToBorder);
				sampler_info.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
				sampler_info.setMagFilter(vk::Filter::eLinear);
				sampler_info.setMinFilter(vk::Filter::eLinear);
				sampler_info.setMinLod(0.f);
				sampler_info.setMaxLod(0.f);
				sampler_info.setMipLodBias(0.f);
				sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
				sampler_info.setUnnormalizedCoordinates(false);
				m_image_arise_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_arise.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

			}

			vk::DescriptorImageInfo samplers[] = {
				vk::DescriptorImageInfo(m_image_density_sampler.get(), m_image_density_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_arise_sampler.get(), m_image_arise_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_shadow_sampler.get(), m_image_shadow_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_render_sampler.get(), m_image_render_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			vk::DescriptorImageInfo images[] = {
				vk::DescriptorImageInfo().setImageView(m_image_density_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_arise_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_shadow_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_render_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
			};
			vk::DescriptorBufferInfo uniforms[] = {
				u_hash.getInfo(),
			};
			vk::DescriptorBufferInfo buffers[] = {
				b_density.getInfo(),
			};


			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(array_length(samplers))
				.setPImageInfo(samplers)
				.setDstBinding(0)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(array_length(images))
				.setPImageInfo(images)
				.setDstBinding(10)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(20)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(buffers))
				.setPBufferInfo(buffers)
				.setDstBinding(30)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		// shader
		{
			const char* name[] =
			{
				"SkyReference.comp.spv",
				"SkyAriseReference.comp.spv",
				"Sky_Render.comp.spv",
				"Sky_CalcDensity.comp.spv",
				"Sky_MakeTexture_PartialX.comp.spv",
				"Sky_MakeTexture_PartialY.comp.spv",
				"Sky_MakeTexture_PartialZ.comp.spv",

				"SkyArise.comp.spv",
				"SkyArise_MakeTexture.comp.spv",

				"Sky_RenderUpsampling.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					RenderTarget::s_descriptor_set_layout.get(),
					m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(12).setStageFlags(vk::ShaderStageFlagBits::eCompute),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_pipeline_layout[PipelineLayout_Sky_CS] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}

		}

		// compute pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 10> shader_info;
			shader_info[0].setModule(m_shader[Shader_SkyReference_CS].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_Sky_Render_CS].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_Sky_CalcDensity_CS].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_Sky_MakeTexture_PartialX_CS].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader[Shader_Sky_MakeTexture_PartialY_CS].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			shader_info[5].setModule(m_shader[Shader_Sky_MakeTexture_PartialZ_CS].get());
			shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[5].setPName("main");
			shader_info[6].setModule(m_shader[Shader_SkyArise_CS].get());
			shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[6].setPName("main");
			shader_info[7].setModule(m_shader[Shader_SkyArise_MakeTexture_CS].get());
			shader_info[7].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[7].setPName("main");
			shader_info[8].setModule(m_shader[Shader_SkyReference_CS].get());
			shader_info[8].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[8].setPName("main");
			shader_info[9].setModule(m_shader[Shader_Sky_RenderUpsampling_CS].get());
			shader_info[9].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[9].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[4])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[5])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[6])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[7])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[8])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[9])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
			};
			auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_SkyReference_CS] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_Sky_Render_CS] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_Sky_CalcDensity_CS] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_Sky_MakeTexture_PartialX_CS] = std::move(compute_pipeline[3]);
			m_pipeline[Pipeline_Sky_MakeTexture_PartialY_CS] = std::move(compute_pipeline[4]);
			m_pipeline[Pipeline_Sky_MakeTexture_PartialZ_CS] = std::move(compute_pipeline[5]);
			m_pipeline[Pipeline_SkyArise_CS] = std::move(compute_pipeline[6]);
			m_pipeline[Pipeline_SkyArise_MakeTexture_CS] = std::move(compute_pipeline[7]);
			m_pipeline[Pipeline_SkyAriseReference_CS] = std::move(compute_pipeline[8]);
			m_pipeline[Pipeline_Sky_RenderUpsampling_CS] = std::move(compute_pipeline[9]);
		}


	}

	void execute_reference(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		auto window = sGlobal::Order().getTotalTime() * 20.f;
		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			cmd.pushConstants<float>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, window);

		}

		// render_targetに書く
		_label.insert("render cloud");
		{
			{
				//				cmd.dispatchBase()
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(render_target->m_image);
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyReference_CS].get());

			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	void execute_AriseReference(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		auto window = sGlobal::Order().getTotalTime() * 20.f;
		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			cmd.pushConstants<float>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, window);

		}

		// render_targetに書く
		_label.insert("render cloud");
		{
			{
				//				cmd.dispatchBase()
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(render_target->m_image);
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyAriseReference_CS].get());

			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}


	void execute(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		auto window = vec3(sGlobal::Order().getTotalTime()) * vec3(20.f, 0.f, 12.f);
		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			cmd.pushConstants<vec3>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, window);

		}

		// make texture
		_label.insert("make cloud map");
		{
#if 1
			{
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_density.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			uvec3 tex_size = uvec3(m_image_density_info.extent.width, m_image_density_info.extent.height, m_image_density_info.extent.depth);
			static bool s_is_init;
			if (!s_is_init)
			{
				// 初回はデータを全部埋める
				s_is_init = true;
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_MakeTexture_PartialX_CS].get());
				auto num = app::calcDipatchGroups(tex_size, uvec3(64, 1, 1));
				cmd.dispatch(num.x, num.y, num.z);
			}
			else 
			{
				static vec3 s_window;
				uvec3 local_size[3] = { uvec3(64, 1, 1), uvec3(4, 16, 1), uvec3(1, 1, 64) };
				uvec3 offset =(uvec3)glm::floor(window) % tex_size;
				uvec3 group[] =
				{
					app::calcDipatchGroups(uvec3(tex_size.x, tex_size.y, 1), uvec3(64, 1, 1)),
					app::calcDipatchGroups(uvec3(tex_size.x, 1, tex_size.z), uvec3(4, 16, 1)),
					app::calcDipatchGroups(uvec3(1, tex_size.y, tex_size.z), uvec3(1, 1, 64)),
				};
				for (INT i = 0; i < 3; i++)
				{
					if (floor(s_window[i]) != floor(window[i]))
					{
						cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_MakeTexture_PartialX_CS+i].get());
						cmd.dispatchBase(i==0?offset.x:0, i==1?offset.y:0, i==2?offset.z:0, group[i].x, group[i].y, group[i].z);
					}
				}
				s_window = window;
			}
#else
			{
				std::array<vk::BufferMemoryBarrier, 1> buffer_barrier = {
					b_density.makeMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(buffer_barrier), buffer_barrier.data() }, {});
			}

			{
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_CalcDensity_CS].get());
				auto tex_size = uvec3(m_image_density_info.extent.width / 16, m_image_density_info.extent.height, m_image_density_info.extent.depth);
				auto num = app::calcDipatchGroups(tex_size, uvec3(32, 1, 2));
				cmd.dispatch(num.x, num.y, num.z);
			}

			{
				std::array<vk::BufferMemoryBarrier, 1> buffer_barrier = {
					b_density.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead),
				};

				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_density.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
 				image_barrier[0].setNewLayout(vk::ImageLayout::eTransferDstOptimal);
 				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_length(buffer_barrier), buffer_barrier.data() }, { array_length(image_barrier), image_barrier.data() });
			}
			
			{
				vk::BufferImageCopy copy;
				copy.setImageSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1));
				copy.setImageOffset(vk::Offset3D(0, 0, 0));
				copy.setImageExtent(m_image_density_info.extent);
				copy.setBufferOffset(b_density.getInfo().offset);
//				cmd.copyBufferToImage(b_density.getInfo().buffer, m_image_density.get(), vk::ImageLayout::eTransferDstOptimal, copy);
			}

#endif
		}
		// render_targetに書く
		_label.insert("render cloud");
		{
			{
				std::array<vk::ImageMemoryBarrier, 2> image_barrier;
				image_barrier[0].setImage(m_image_render.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setImage(m_image_density.get());
				image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_Render_CS].get());
			auto num = app::calcDipatchGroups(uvec3(512, 512, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
		{
			_executeUpsampling(cmd, render_target);
		}
	}


	void executeArise(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);
		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			cmd.pushConstants<float>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, sGlobal::Order().getTotalTime());

		}

		// make texture
		_label.insert("make cloud map");
		{
			{

				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_arise.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}


			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyArise_MakeTexture_CS].get());

			auto num = app::calcDipatchGroups(uvec3(m_image_arise_info.extent.width, m_image_arise_info.extent.height, m_image_arise_info.extent.depth), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// render_targetに書く
		_label.insert("render cloud");
		{
			{

				{

					std::array<vk::ImageMemoryBarrier, 2> image_barrier;
					image_barrier[0].setImage(m_image_render.get());
					image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
					image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
					image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
					image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
					image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
					image_barrier[1].setImage(m_image_arise.get());
					image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
					image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
					image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
					image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
					image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
				}

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyArise_CS].get());

				auto num = app::calcDipatchGroups(uvec3(m_image_render_info.extent.width, m_image_render_info.extent.height, 1), uvec3(32, 32, 1));
				cmd.dispatch(num.x, num.y, num.z);
			}

			{
				_executeUpsampling(cmd, render_target);
			}
		}
	}
	void _executeUpsampling(vk::CommandBuffer &cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		{
			std::array<vk::ImageMemoryBarrier, 2> image_barrier;
			image_barrier[0].setImage(render_target->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
			image_barrier[1].setImage(m_image_render.get());
			image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
			image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
			image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_RenderUpsampling_CS].get());

		auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

};
int main()
{
	btr::setResourceAppPath("..\\..\\resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(0.f, -500.f, 800.f);
	camera->getData().m_target = vec3(0.f, -100.f, 0.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;
	{
		auto dir = normalize(vec3(1.f, 3.f, 2.f));
		auto rot = normalize(cross(vec3(0.f, 1.f, 0.f), normalize(vec3(dir.x, 0.f, dir.z))));
		int a = 0;
	}

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);
	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());
	Sky sky(context, app.m_window->getFrontBuffer());
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_sky,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_sky");
//				sky.execute_reference(cmd, app.m_window->getFrontBuffer());
//				sky.execute_AriseReference(cmd, app.m_window->getFrontBuffer());
				sky.executeArise(cmd, app.m_window->getFrontBuffer());
				cmd.end();
				cmds[cmd_sky] = cmd;
			}

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			app.submit(std::move(cmds));
		}

		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

