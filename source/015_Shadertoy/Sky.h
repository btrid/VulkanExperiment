#pragma once

struct SkyNoise
{
	enum
	{
		TILE_SIZE = 32,
	};
	SkyNoise(const std::shared_ptr<btr::Context>& context)
	{

		// descriptor layout
		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
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
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(128, 128, 128));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR8G8B8A8Unorm);
				image_info.setImageType(vk::ImageType::e3D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_cloud = context->m_device.createImageUnique(image_info);
				m_image_cloud_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_cloud.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_cloud_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_cloud.get(), m_image_cloud_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_cloud.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e3D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eG).setB(vk::ComponentSwizzle::eB).setA(vk::ComponentSwizzle::eA);
				m_image_cloud_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8G8B8A8Uint);
				m_image_cloud_write_view = context->m_device.createImageViewUnique(view_info);

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
				m_image_cloud_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_cloud.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });
			}
			{
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(32, 32, 32));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR8G8B8A8Unorm);
				image_info.setImageType(vk::ImageType::e3D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_cloud_detail = context->m_device.createImageUnique(image_info);
				m_image_cloud_detail_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_cloud_detail.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_cloud_detail_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_cloud_detail.get(), m_image_cloud_detail_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_cloud_detail.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e3D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eG).setB(vk::ComponentSwizzle::eB).setA(vk::ComponentSwizzle::eIdentity);
				m_image_cloud_detail_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8G8B8A8Uint);
				m_image_cloud_detail_write_view = context->m_device.createImageViewUnique(view_info);

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
				m_image_cloud_detail_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_cloud_detail.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });
			}

			{
				vk::ImageCreateInfo image_info;
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

				m_image_weather = context->m_device.createImageUnique(image_info);
				m_image_weather_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_weather.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_weather_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_weather.get(), m_image_weather_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_weather.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e2D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eG).setB(vk::ComponentSwizzle::eB).setA(vk::ComponentSwizzle::eIdentity);
				m_image_weather_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8G8B8A8Uint);
				m_image_weather_write_view = context->m_device.createImageViewUnique(view_info);

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
				m_image_weather_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_weather.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });
			}

			{
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(1024, 1024, 1));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR8G8Unorm);
				image_info.setImageType(vk::ImageType::e2D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_cloud_distort = context->m_device.createImageUnique(image_info);
				m_image_cloud_distort_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_cloud_distort.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_cloud_distort_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_cloud_distort.get(), m_image_cloud_distort_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_cloud_distort.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e2D);
				view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eG).setB(vk::ComponentSwizzle::eB).setA(vk::ComponentSwizzle::eIdentity);
				m_image_cloud_distort_view = context->m_device.createImageViewUnique(view_info);

				view_info.setFormat(vk::Format::eR8G8Uint);
				m_image_cloud_distort_write_view = context->m_device.createImageViewUnique(view_info);

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
				m_image_cloud_distort_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_cloud_distort.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });
			}
			vk::DescriptorImageInfo samplers[] = {
				vk::DescriptorImageInfo(m_image_cloud_sampler.get(), m_image_cloud_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_cloud_detail_sampler.get(), m_image_cloud_detail_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_cloud_distort_sampler.get(), m_image_cloud_distort_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_weather_sampler.get(), m_image_weather_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			vk::DescriptorImageInfo images[] = {
				vk::DescriptorImageInfo().setImageView(m_image_cloud_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_cloud_detail_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_cloud_distort_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_weather_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
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
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		// shader
		{
			const char* name[] =
			{
				"WorleyNoise_Compute.comp.spv",
				"WorleyNoise_ComputeWeatherTexture.comp.spv",
				"WorleyNoise_ComputeDistortTexture.comp.spv",
				"WorleyNoise_Render.comp.spv",
			};
			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_WorleyNoise_CS] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_WorleyNoise_Render] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}
		// compute pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 4> shader_info;
			shader_info[0].setModule(m_shader[Shader_WorleyNoise_Compute].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_WorleyNoise_ComputeWeatherTexture].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_WorleyNoise_ComputeDistortTexture].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_WorleyNoise_Render].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_WorleyNoise_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_WorleyNoise_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_WorleyNoise_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_WorleyNoise_Render].get()),
			};
			auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_WorleyNoise_Compute] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_WorleyNoise_ComputeWeatherTexture] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_WorleyNoise_ComputeDistortTexture] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_WorleyNoise_Render] = std::move(compute_pipeline[3]);
		}
	}

	void execute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd)
	{
		DebugLabel _label(cmd, context->m_dispach, __FUNCTION__);

		{
			vk::DescriptorSet descs[] =
			{
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_WorleyNoise_CS].get(), 0, array_length(descs), descs, 0, nullptr);
		}

		_label.insert("compute");
		{
			{
				std::array<vk::ImageMemoryBarrier, 4> image_barrier;
				image_barrier[0].setImage(m_image_cloud.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setImage(m_image_cloud_detail.get());
				image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[1].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[1].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[2].setImage(m_image_cloud_distort.get());
				image_barrier[2].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[2].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[2].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[2].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[2].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[3].setImage(m_image_weather.get());
				image_barrier[3].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[3].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[3].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[3].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[3].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			{
				uvec3 num = app::calcDipatchGroups(uvec3(m_image_cloud_info.extent.width, m_image_cloud_info.extent.height, m_image_cloud_info.extent.depth), uvec3(32, 32, 1));
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_WorleyNoise_Compute].get());
				cmd.dispatch(num.x, num.y, num.z);

			}
			{
				uvec3 num = app::calcDipatchGroups(uvec3(m_image_cloud_distort_info.extent.width, m_image_cloud_distort_info.extent.height, m_image_cloud_distort_info.extent.depth), uvec3(32, 32, 1));
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_WorleyNoise_ComputeDistortTexture].get());
				cmd.dispatch(num.x, num.y, num.z);

			}
			{
				uvec3 num = app::calcDipatchGroups(uvec3(m_image_weather_info.extent.width, m_image_weather_info.extent.height, m_image_weather_info.extent.depth), uvec3(32, 32, 1));
				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_WorleyNoise_ComputeWeatherTexture].get());
				cmd.dispatch(num.x, num.y, num.z);

			}

			{
				std::array<vk::ImageMemoryBarrier, 4> image_barrier;
				image_barrier[0].setImage(m_image_cloud.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[0].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[1].setImage(m_image_cloud_detail.get());
				image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[2].setImage(m_image_cloud_distort.get());
				image_barrier[2].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[2].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[2].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[2].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[2].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[3].setImage(m_image_weather.get());
				image_barrier[3].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[3].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[3].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[3].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[3].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

		}
	}

	void execute_Render(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, context->m_dispach, __FUNCTION__);

		{
			vk::DescriptorSet descs[] =
			{
				m_descriptor_set.get(),
				render_target->m_descriptor.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_WorleyNoise_Render].get(), 0, array_length(descs), descs, 0, nullptr);
		}

		{
			{
				std::array<vk::ImageMemoryBarrier, 2> image_barrier;
				image_barrier[0].setImage(render_target->m_image);
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setImage(m_image_cloud.get());
				image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				//				image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
				//				image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });

			}


			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_WorleyNoise_Render].get());
			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	enum Shader
	{
		Shader_WorleyNoise_Compute,
		Shader_WorleyNoise_ComputeWeatherTexture,
		Shader_WorleyNoise_ComputeDistortTexture,
		Shader_WorleyNoise_Render,
		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_WorleyNoise_CS,
		PipelineLayout_WorleyNoise_Render,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_WorleyNoise_Compute,
		Pipeline_WorleyNoise_ComputeWeatherTexture,
		Pipeline_WorleyNoise_ComputeDistortTexture,
		Pipeline_WorleyNoise_Render,
		Pipeline_Num,
	};

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;


	vk::ImageCreateInfo m_image_cloud_info;
	vk::UniqueImage m_image_cloud;
	vk::UniqueImageView m_image_cloud_view;
	vk::UniqueImageView m_image_cloud_write_view;
	vk::UniqueDeviceMemory m_image_cloud_memory;
	vk::UniqueSampler m_image_cloud_sampler;

	vk::ImageCreateInfo m_image_cloud_detail_info;
	vk::UniqueImage m_image_cloud_detail;
	vk::UniqueImageView m_image_cloud_detail_view;
	vk::UniqueImageView m_image_cloud_detail_write_view;
	vk::UniqueDeviceMemory m_image_cloud_detail_memory;
	vk::UniqueSampler m_image_cloud_detail_sampler;

	vk::ImageCreateInfo m_image_weather_info;
	vk::UniqueImage m_image_weather;
	vk::UniqueImageView m_image_weather_view;
	vk::UniqueImageView m_image_weather_write_view;
	vk::UniqueDeviceMemory m_image_weather_memory;
	vk::UniqueSampler m_image_weather_sampler;

	vk::ImageCreateInfo m_image_cloud_distort_info;
	vk::UniqueImage m_image_cloud_distort;
	vk::UniqueImageView m_image_cloud_distort_view;
	vk::UniqueImageView m_image_cloud_distort_write_view;
	vk::UniqueDeviceMemory m_image_cloud_distort_memory;
	vk::UniqueSampler m_image_cloud_distort_sampler;
};
struct Sky
{
	struct Constant
	{
		vec4 window;
		vec4 light_front;
	};
	enum Shader
	{
		Shader_SkyReference_CS,


		Shader_Sky_Render_CS,
		Shader_Sky_RenderShadow_CS,

		Shader_Sky_Precompute_Shadow_CS,
		Shader_Sky_Precompute_PowderEffect_CS,

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

		Pipeline_Sky_Render_CS,
		Pipeline_Sky_RenderShadow_CS,

		Pipeline_Sky_Precompute_Shadow_CS,
		Pipeline_Sky_Precompute_PowderEffect_CS,

		Pipeline_Sky_RenderUpsampling_CS,
		Pipeline_Num,
	};

	std::shared_ptr<btr::Context> m_context;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::ImageCreateInfo m_image_shadow_info;
	vk::UniqueImage m_image_shadow;
	vk::UniqueImageView m_image_shadow_view;
	vk::UniqueImageView m_image_shadow_write_view;
	vk::UniqueDeviceMemory m_image_shadow_memory;
	vk::UniqueSampler m_image_shadow_sampler;

	vk::ImageCreateInfo m_image_along_density_info;
	vk::UniqueImage m_image_along_density;
	vk::UniqueImageView m_image_along_density_view;
	vk::UniqueImageView m_image_along_density_write_view;
	vk::UniqueDeviceMemory m_image_along_density_memory;
	vk::UniqueSampler m_image_along_density_sampler;

	vk::ImageCreateInfo m_image_render_info;
	vk::UniqueImage m_image_render;
	vk::UniqueImageView m_image_render_view;
	vk::UniqueImageView m_image_render_write_view;
	vk::UniqueDeviceMemory m_image_render_memory;
	vk::UniqueSampler m_image_render_sampler;

	SkyNoise m_skynoise;
	Sky(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
		: m_skynoise(context)
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
				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageImage, 1, stage),
				vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eStorageImage, 1, stage),
				vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eStorageImage, 1, stage),
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
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(256 * 4, 32, 256 * 4));
				image_info.setArrayLayers(1);
				//				image_info.setFormat(vk::Format::eR8Unorm);
				image_info.setFormat(vk::Format::eR16Unorm);
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
				m_image_shadow_view = context->m_device.createImageViewUnique(view_info);

				//				view_info.setFormat(vk::Format::eR8Uint);
				view_info.setFormat(vk::Format::eR16Uint);
				m_image_shadow_write_view = context->m_device.createImageViewUnique(view_info);

				vk::SamplerCreateInfo sampler_info;
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
				//				sampler_info.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
				sampler_info.setMagFilter(vk::Filter::eLinear);
				sampler_info.setMinFilter(vk::Filter::eLinear);
				sampler_info.setMinLod(0.f);
				sampler_info.setMaxLod(0.f);
				sampler_info.setMipLodBias(0.f);
				sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
				//				sampler_info.setUnnormalizedCoordinates(false);
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
				image_info.setExtent(vk::Extent3D(256 * 4, 32, 256 * 4));
				image_info.setArrayLayers(1);
				image_info.setFormat(vk::Format::eR16Unorm);
				image_info.setImageType(vk::ImageType::e3D);
				image_info.setInitialLayout(vk::ImageLayout::eUndefined);
				image_info.setMipLevels(1);
				image_info.setSamples(vk::SampleCountFlagBits::e1);
				image_info.setSharingMode(vk::SharingMode::eExclusive);
				image_info.setTiling(vk::ImageTiling::eOptimal);
				image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage);
				image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

				m_image_along_density = context->m_device.createImageUnique(image_info);
				m_image_along_density_info = image_info;

				vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image_along_density.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_image_along_density_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
				context->m_device.bindImageMemory(m_image_along_density.get(), m_image_along_density_memory.get(), 0);

				vk::ImageViewCreateInfo view_info;
				view_info.setFormat(image_info.format);
				view_info.setImage(m_image_along_density.get());
				view_info.subresourceRange.setBaseArrayLayer(0);
				view_info.subresourceRange.setLayerCount(1);
				view_info.subresourceRange.setBaseMipLevel(0);
				view_info.subresourceRange.setLevelCount(1);
				view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
				view_info.setViewType(vk::ImageViewType::e3D);
				m_image_along_density_view = context->m_device.createImageViewUnique(view_info);

				//				view_info.setFormat(vk::Format::eR8Uint);
				view_info.setFormat(vk::Format::eR16Uint);
				m_image_along_density_write_view = context->m_device.createImageViewUnique(view_info);

				vk::SamplerCreateInfo sampler_info;
				sampler_info.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
				sampler_info.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
				sampler_info.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
				//				sampler_info.setBorderColor(vk::BorderColor::eFloatOpaqueWhite);
				sampler_info.setMagFilter(vk::Filter::eLinear);
				sampler_info.setMinFilter(vk::Filter::eLinear);
				sampler_info.setMinLod(0.f);
				sampler_info.setMaxLod(0.f);
				sampler_info.setMipLodBias(0.f);
				sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
				//				sampler_info.setUnnormalizedCoordinates(false);
				m_image_along_density_sampler = context->m_device.createSamplerUnique(sampler_info);


				vk::ImageMemoryBarrier to_make_barrier;
				to_make_barrier.image = m_image_along_density.get();
				to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

			}
			{
				vk::ImageCreateInfo image_info;
				image_info.setExtent(vk::Extent3D(render_target->m_info.extent.width / 2, render_target->m_info.extent.height / 2, 1));
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
			vk::DescriptorImageInfo samplers[] = {
				vk::DescriptorImageInfo(m_image_shadow_sampler.get(), m_image_shadow_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_along_density_sampler.get(), m_image_along_density_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::DescriptorImageInfo(m_image_render_sampler.get(), m_image_render_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			vk::DescriptorImageInfo images[] = {
				vk::DescriptorImageInfo().setImageView(m_image_shadow_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_along_density_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
				vk::DescriptorImageInfo().setImageView(m_image_render_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral),
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
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		// shader
		{
			const char* name[] =
			{
				"SkyReference.comp.spv",

				"Sky_Render.comp.spv",
				"Sky_RenderShadow.comp.spv",

				"Sky_Precompute_Shadow.comp.spv",
				"Sky_Precompute_PowderEffect.comp.spv",

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
					m_skynoise.m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(64).setStageFlags(vk::ShaderStageFlagBits::eCompute),
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
			std::array<vk::PipelineShaderStageCreateInfo, 6> shader_info;
			shader_info[0].setModule(m_shader[Shader_SkyReference_CS].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_Sky_Render_CS].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_Sky_RenderShadow_CS].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_Sky_Precompute_Shadow_CS].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader[Shader_Sky_Precompute_PowderEffect_CS].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			shader_info[5].setModule(m_shader[Shader_Sky_RenderUpsampling_CS].get());
			shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[5].setPName("main");
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
			};
			auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_SkyReference_CS] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_Sky_Render_CS] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_Sky_RenderShadow_CS] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_Sky_Precompute_Shadow_CS] = std::move(compute_pipeline[3]);
			m_pipeline[Pipeline_Sky_Precompute_PowderEffect_CS] = std::move(compute_pipeline[4]);
			m_pipeline[Pipeline_Sky_RenderUpsampling_CS] = std::move(compute_pipeline[5]);
		}


	}

	Constant calc_constant()const
	{
		float c = cos(sGlobal::Order().getTotalTime());
		float s = sin(sGlobal::Order().getTotalTime());
//		auto LightRay = normalize(vec3(s, c, 0.1));
		auto LightRay = normalize(vec3(0.f, -1.f, 0.f));
		Constant constant;
		constant.window = vec4(sGlobal::Order().getTotalTime()) * vec4(1.f, 0.f, 12.f, 0.f);
		constant.light_front = vec4(LightRay, 0.f);
		return constant;

	}
	void execute_reference(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
//				m_skynoise.m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			auto constant = calc_constant();
			cmd.pushConstants<Constant>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);

		}

		// render_targetÇ…èëÇ≠
		_label.insert("render cloud");
		{
			{
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_render.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyReference_CS].get());

//			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
			auto num = app::calcDipatchGroups(uvec3(512, 512, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
		{
			_executeUpsampling(cmd, render_target);
		}


	}

	void execute(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
				m_skynoise.m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			auto constant = calc_constant();
			cmd.pushConstants<Constant>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);

		}

		// render_targetÇ…èëÇ≠
		_label.insert("render cloud");
		{
			{
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_render.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_Render_CS].get());
			auto num = app::calcDipatchGroups(uvec3(512, 512, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

			{
				_executeUpsampling(cmd, render_target);
			}
		}
	}

	void execute_precompute(vk::CommandBuffer &cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
				m_skynoise.m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			auto constant = calc_constant();
			cmd.pushConstants<Constant>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);

		}

		_label.insert("precompute along density");
		{
			{
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_along_density.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_Precompute_PowderEffect_CS].get());
			auto num = app::calcDipatchGroups(uvec3(m_image_along_density_info.extent.width, m_image_along_density_info.extent.depth, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

			{
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_along_density.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[0].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}
		}

		// test
		if (0)
		{
// 			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
// 			image_barrier[1].setImage(render_target->m_image);
// 			image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
// 			image_barrier[1].setNewLayout(vk::ImageLayout::eGeneral);
// 			image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
// 
// 			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
// 
// 			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_RenderShadow_CS].get());
// 			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
// 			cmd.dispatch(num.x, num.y, num.z);

		}
	}
	void executeShadow(vk::CommandBuffer &cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
				m_skynoise.m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			auto constant = calc_constant();
			cmd.pushConstants<Constant>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, constant);

		}

		// render_targetÇ…èëÇ≠
		_label.insert("make transmittance");
		{
			{
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_shadow.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_Precompute_Shadow_CS].get());
			auto num = app::calcDipatchGroups(uvec3(m_image_shadow_info.extent.width, m_image_shadow_info.extent.depth, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

			{
				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image_shadow.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[0].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });

			}
		}

		// test
		if(0)
		{
			std::array<vk::ImageMemoryBarrier, 0> image_barrier;
			image_barrier[0].setImage(render_target->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Sky_RenderShadow_CS].get());
			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

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