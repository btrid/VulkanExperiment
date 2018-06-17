#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/PM2D/PM2D.h>

namespace pm2d
{

struct PM2DRT
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
		ShaderMakePRT,
		ShaderRendering,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakePRT,
		PipelineLayoutRendering,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakePRT,
		PipelineRendering,
		PipelineNum,
	};

	PM2DRT(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PM2DContext>& pm2d_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = context;
		m_pm2d_context = pm2d_context;
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
			image_info.extent = { (uint32_t)pm2d_context->RenderWidth, (uint32_t)pm2d_context->RenderHeight, 1 };
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
			{
				uint32_t map = (m_pm2d_context->RenderWidth*m_pm2d_context->RenderHeight) / 64 / 4;
				uint32_t mapp = map/4 * map;
				b_prt_data = m_context->m_storage_memory.allocateMemory<uint32_t>({ mapp, {} });
			}

			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
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
					b_prt_data.getInfo(),
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
				"ComputeRadianceTransfer.comp.spv",
				"RTRendering.comp.spv",
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
				pm2d_context->getDescriptorSetLayout(),
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
			std::array<vk::PipelineShaderStageCreateInfo, 1> shader_info;
			shader_info[0].setModule(m_shader[ShaderMakePRT].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayoutMakePRT].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PipelineMakePRT] = std::move(compute_pipeline[0]);
		}

	}
	void execute(vk::CommandBuffer cmd)
	{
		static int a = 0;
		if (a == 0) 
		{
			// Ž–‘OŒvŽZ
			a++;
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakePRT].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakePRT].get(), 0, m_pm2d_context->getDescriptorSet(), {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakePRT].get(), 1, m_descriptor_set.get(), {});
			auto num = app::calcDipatchGroups(uvec3(m_pm2d_context->RenderWidth / 32 / 8, m_pm2d_context->RenderHeight / 32 / 8, 1), uvec3(32, 32, 1));

			auto yy = m_pm2d_context->RenderHeight / 8;
			auto xx = m_pm2d_context->RenderWidth / 8;
			for (int y = 0; y < yy; y++)
			{
				for (int x = 0; x < xx; x++)
				{
					cmd.pushConstants<ivec2>(m_pipeline_layout[PipelineLayoutMakePRT].get(), vk::ShaderStageFlagBits::eCompute, 0, ivec2(y*xx + x, 0));
					cmd.dispatch(num.x, num.y, num.z);
				}

			}
		}


	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PM2DContext> m_pm2d_context;
	std::shared_ptr<RenderTarget> m_render_target;

	std::array<vk::UniqueShaderModule, ShaderNum> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	TextureResource m_color_tex;
	btr::BufferMemoryEx<uint32_t> b_prt_data;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;


};



}