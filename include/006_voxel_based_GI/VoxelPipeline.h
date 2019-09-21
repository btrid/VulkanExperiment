#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <applib/App.h>

struct VoxelInfo
{
	vec4 u_area_min;
	vec4 u_area_max;
	vec4 u_cell_size;
	uvec4 u_cell_num;

	VoxelInfo()
	{
		u_area_max = vec4(1000.f);
		u_area_min = vec4(-1000.f);
		u_cell_num = uvec4(32);
		u_cell_size = (u_area_max - u_area_min) / vec4(u_cell_num);
	}
};

struct VoxelContext
{
	VoxelContext(std::shared_ptr<btr::Context>& context, const VoxelInfo& info)
	{
		m_voxelize_info_cpu = info;
		int mipnum = 5;
		auto& device = context->m_device;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		// resource setup
		{
			m_voxel_info = context->m_uniform_memory.allocateMemory<VoxelInfo>({ 1, {} });
			auto staging = context->m_staging_memory.allocateMemory<VoxelInfo>({ 1, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });

			*staging.getMappedPtr() = m_voxelize_info_cpu;

			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(m_voxel_info.getInfo().offset);
			copy.setSize(staging.getInfo().range);
			cmd.copyBuffer(staging.getInfo().buffer, m_voxel_info.getInfo().buffer, copy);
		}

		{
			// Create descriptor set layout
			vk::DescriptorSetLayoutBinding bindings[] =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(8)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setBinding(2),
			};
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
			descriptor_layout_info.setBindingCount(array_length(bindings));
			descriptor_layout_info.setPBindings(bindings);
			m_descriptor_set_layout = device->createDescriptorSetLayoutUnique(descriptor_layout_info);
		}

		// descriptor set
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = context->m_descriptor_pool.get();
			alloc_info.descriptorSetCount = array_length(layouts);
			alloc_info.pSetLayouts = layouts;
			m_descriptor_set = std::move(device->allocateDescriptorSetsUnique(alloc_info)[0]);

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_voxel_info.getInfo(),
			};

			std::vector<vk::WriteDescriptorSet> write_descriptor_set =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
			};
			device->updateDescriptorSets(write_descriptor_set, {});
		}
	}
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	VoxelInfo m_voxelize_info_cpu;
	btr::BufferMemoryEx<VoxelInfo> m_voxel_info;
	btr::BufferMemoryEx<uint32_t> m_albedo;
	btr::BufferMemoryEx<uint32_t> m_normal;

	const VoxelInfo& getVoxelInfo()const { return m_voxelize_info_cpu; }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }

};
struct VoxelContext_Old
{
	VoxelContext_Old(std::shared_ptr<btr::Context>& context, const VoxelInfo& info)
	{
		m_voxelize_info_cpu = info;
		int mipnum = 5;
		auto& device = context->m_device;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		// resource setup
		{
			m_voxel_info = context->m_uniform_memory.allocateMemory<VoxelInfo>({ 1, {} });
			auto staging = context->m_staging_memory.allocateMemory<VoxelInfo>({ 1, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });

			*staging.getMappedPtr() = m_voxelize_info_cpu;

			vk::BufferCopy copy;
			copy.setSrcOffset(staging.getInfo().offset);
			copy.setDstOffset(m_voxel_info.getInfo().offset);
			copy.setSize(staging.getInfo().range);
			cmd.copyBuffer(staging.getInfo().buffer, m_voxel_info.getInfo().buffer, copy);
		}

		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e3D;
			image_info.format = vk::Format::eR8G8B8A8Unorm;
			image_info.mipLevels = mipnum;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = { m_voxelize_info_cpu.u_cell_num.x, m_voxelize_info_cpu.u_cell_num.y, m_voxelize_info_cpu.u_cell_num.z };
			auto image = device->createImageUnique(image_info);

			vk::MemoryRequirements memory_request = device->getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			auto image_memory = device->allocateMemoryUnique(memory_alloc_info);
			device->bindImageMemory(image.get(), image_memory.get(), 0);

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = 1;
			subresourceRange.levelCount = image_info.mipLevels;

			{
				// ‰ŠúÝ’è
				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_copy_barrier.subresourceRange = subresourceRange;

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_copy_barrier });

			}

			m_voxel_hierarchy_image = std::move(image);
			m_voxel_hierarchy_imagememory = std::move(image_memory);

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::e3D;
			view_info.components.r = vk::ComponentSwizzle::eR;
			view_info.components.g = vk::ComponentSwizzle::eG;
			view_info.components.b = vk::ComponentSwizzle::eB;
			view_info.components.a = vk::ComponentSwizzle::eA;
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = m_voxel_hierarchy_image.get();
			view_info.subresourceRange = subresourceRange;

			m_voxel_imageview = device->createImageViewUnique(view_info);

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

			m_voxel_sampler = context->m_device.createSamplerUnique(sampler_info);

			view_info.subresourceRange.levelCount = 1;
			m_voxel_hierarchy_imageview.resize(mipnum);
			for (int i = 0; i < mipnum; i++)
			{
				view_info.subresourceRange.baseMipLevel = i;
				m_voxel_hierarchy_imageview[i] = device->createImageViewUnique(view_info);
			}

		}

		{
			// Create descriptor set layout
			vk::DescriptorSetLayoutBinding bindings[] = 
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(8)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setBinding(2),
			};
			vk::DescriptorSetLayoutCreateInfo descriptor_layout_info;
			descriptor_layout_info.setBindingCount(array_length(bindings));
			descriptor_layout_info.setPBindings(bindings);
			m_descriptor_set_layout = device->createDescriptorSetLayoutUnique(descriptor_layout_info);
		}

		// descriptor set
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = context->m_descriptor_pool.get();
			alloc_info.descriptorSetCount = array_length(layouts);
			alloc_info.pSetLayouts = layouts;
			m_descriptor_set = std::move(device->allocateDescriptorSetsUnique(alloc_info)[0]);

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_voxel_info.getInfo(),
			};

			auto param = vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_voxel_hierarchy_imageview[0].get());
			std::vector<vk::DescriptorImageInfo> textures(8, param);
			for (int i = 0; i < mipnum; i++)
			{
				textures[i].setImageView(m_voxel_hierarchy_imageview[i].get());
			}

			std::vector<vk::DescriptorImageInfo> samplers = {
				vk::DescriptorImageInfo()
				.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
				.setImageView(m_voxel_imageview.get())
				.setSampler(m_voxel_sampler.get())
			};

			std::vector<vk::WriteDescriptorSet> write_descriptor_set =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(samplers.size())
				.setPImageInfo(samplers.data())
				.setDstBinding(2)
				.setDstSet(m_descriptor_set.get()),
			};
			for (int i = 0; i < textures.size(); i++)
			{
				vk::WriteDescriptorSet tex = vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(1)
					.setPImageInfo(&textures[i])
					.setDstBinding(1)
					.setDstArrayElement(i)
					.setDstSet(m_descriptor_set.get());
				write_descriptor_set.push_back(tex);
			}
			device->updateDescriptorSets(write_descriptor_set, {});
		}
	}
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	VoxelInfo m_voxelize_info_cpu;
	btr::BufferMemoryEx<VoxelInfo> m_voxel_info;


	vk::UniqueImage m_voxel_hierarchy_image;
	vk::UniqueDeviceMemory m_voxel_hierarchy_imagememory;
	std::vector<vk::UniqueImageView> m_voxel_hierarchy_imageview;
	vk::UniqueImageView m_voxel_imageview;
	vk::UniqueSampler m_voxel_sampler;

	const VoxelInfo& getVoxelInfo()const { return m_voxelize_info_cpu; }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }

};

struct VoxelPipeline
{
	enum SHADER
	{
		SHADER_MAKE_VOXEL_HIERARCHY_COMPUTE,
		SHADER_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_MAKE_HIERARCHY_VOXEL,
		PIPELINE_NUM,
	};

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<VoxelContext_Old> m_vx_context;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	VoxelPipeline(std::shared_ptr<btr::Context>& context, std::shared_ptr<VoxelContext_Old>& vx_context)
	{
		m_context = context;
		m_vx_context = vx_context;

		auto& gpu = context->m_gpu;
		auto& device = gpu.getDevice();

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "MakeVoxelHierarchy.comp.spv",vk::ShaderStageFlagBits::eCompute },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceShaderPath() + "../006_voxel_based_GI/binary/";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_list[i] = std::move(loadShaderUnique(device.get(), path + shader_info[i].name));
				m_stage_info[i].setStage(shader_info[i].stage);
				m_stage_info[i].setModule(m_shader_list[i].get());
				m_stage_info[i].setPName("main");
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					vx_context->getDescriptorSetLayout(),
				};
				vk::PushConstantRange constants[] = {
					vk::PushConstantRange()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setSize(4)
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
				pipeline_layout_info.setPPushConstantRanges(constants);
				m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// pipeline
		{
			{
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(m_stage_info[SHADER_MAKE_VOXEL_HIERARCHY_COMPUTE])
					.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL].get()),
				};
				auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
				m_pipeline[PIPELINE_MAKE_HIERARCHY_VOXEL] = std::move(compute_pipeline[0]);

			}
		}

	}

	void execute(vk::CommandBuffer cmd)
	{
		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(5);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);

		{
			vk::ImageMemoryBarrier to_read_barrier;
			to_read_barrier.image = m_vx_context->m_voxel_hierarchy_image.get();
			to_read_barrier.newLayout = vk::ImageLayout::eGeneral;
			to_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			to_read_barrier.subresourceRange = range;
			to_read_barrier.oldLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
		}

	}
	void executeMakeHierarchy(vk::CommandBuffer cmd)
	{
		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(1);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);

		vk::ImageMemoryBarrier to_read_barrier;
		to_read_barrier.image = m_vx_context->m_voxel_hierarchy_image.get();
		to_read_barrier.oldLayout = vk::ImageLayout::eGeneral;
		to_read_barrier.newLayout = vk::ImageLayout::eGeneral;
		to_read_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
		to_read_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		to_read_barrier.subresourceRange = range;

		auto groups = m_vx_context->m_voxelize_info_cpu.u_cell_num;
		for (int32_t i = 0; i < 5; i++)
		{
			to_read_barrier.subresourceRange.baseMipLevel = i;
			if (i == 0)
			{
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
			}
			else
			{
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_read_barrier });
			}
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_MAKE_HIERARCHY_VOXEL].get());
			vk::ArrayProxy<const vk::DescriptorSet> descriptor_sets = {
				m_vx_context->getDescriptorSet(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL].get(), 0, descriptor_sets, {});
			cmd.pushConstants<int32_t>(m_pipeline_layout[PIPELINE_LAYOUT_MAKE_HIERARCHY_VOXEL].get(), vk::ShaderStageFlagBits::eCompute, 0, i);
			groups /= 2;
			groups = glm::max(groups, uvec4(1));
			cmd.dispatch(groups.x, groups.y, groups.z);
		}
	}
};