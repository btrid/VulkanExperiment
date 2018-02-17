#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>
#include <999_game/MazeGenerator.h>
#include <applib/Geometry.h>
#include <applib/sCameraManager.h>
#include <999_game/sLightSystem.h>

#include <btrlib/VoxelPipeline.h>

struct MapDescriptor
{
	glm::vec2 m_cell_size;
	glm::uvec2 m_cell_num;
};

struct MapInfo
{
	MapDescriptor m_descriptor[2];
	glm::uvec2 m_subcell;
};
struct SceneData
{
	float m_deltatime;
	float m_totaltime;
};


struct sScene : public Singleton<sScene>
{
	enum Shader
	{
		SHADER_COMPUTE_MAP_UPDATE,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MAP,
		DESCRIPTOR_SET_LAYOUT_SCENE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_MAP,
		DESCRIPTOR_SET_SCENE,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_UPDATE_MAP,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_COMPUTE_UPDATE_MAP,
		PIPELINE_NUM,
	};


	MapInfo m_map_info_cpu;

	MazeGenerator m_maze;
	Geometry m_maze_geometry;

	vk::UniqueImage m_map_image;
	vk::UniqueImageView m_map_image_view;
	vk::UniqueDeviceMemory m_map_image_memory;
	vk::UniqueImage m_submap_image;
	vk::UniqueImageView m_submap_image_view;
	vk::UniqueDeviceMemory m_submap_image_memory;
	vk::UniqueImage m_map_damae_image;
	vk::UniqueImageView m_map_damae_image_view;
	vk::UniqueDeviceMemory m_map_damae_image_memory;

	vk::UniqueImage m_map_emission_image;
	vk::UniqueImageView m_map_emission_image_view;
	vk::UniqueDeviceMemory m_map_emission_image_memory;


	btr::BufferMemory m_map_info;
	btr::BufferMemory m_scene_data;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	void setup(std::shared_ptr<btr::Context>& context)
	{
		m_map_info_cpu.m_subcell = glm::uvec2(4, 4);
		m_map_info_cpu.m_descriptor[1].m_cell_size = glm::vec2(50.f, 50.f);
		m_map_info_cpu.m_descriptor[1].m_cell_num = glm::vec2(15, 15);
		m_map_info_cpu.m_descriptor[0].m_cell_size = m_map_info_cpu.m_descriptor[1].m_cell_size / glm::vec2(m_map_info_cpu.m_subcell);
		m_map_info_cpu.m_descriptor[0].m_cell_num = m_map_info_cpu.m_descriptor[1].m_cell_num*m_map_info_cpu.m_subcell;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);


		{
			{
				vk::ImageCreateInfo image_info;
				image_info.imageType = vk::ImageType::e2D;
				image_info.format = vk::Format::eR8Uint;
				image_info.mipLevels = 1;
				image_info.arrayLayers = 1;
				image_info.samples = vk::SampleCountFlagBits::e1;
				image_info.tiling = vk::ImageTiling::eOptimal;
				image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
				image_info.sharingMode = vk::SharingMode::eExclusive;
				image_info.initialLayout = vk::ImageLayout::eUndefined;
				image_info.extent = { m_map_info_cpu.m_descriptor[0].m_cell_num.x, m_map_info_cpu.m_descriptor[0].m_cell_num.y, 1u };
				auto image = context->m_device->createImageUnique(image_info);

				vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(image.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
				auto image_memory = context->m_device->allocateMemoryUnique(memory_alloc_info);
				context->m_device->bindImageMemory(image.get(), image_memory.get(), 0);

				vk::ImageCreateInfo image_damage_info = image_info;
				image_damage_info.format = vk::Format::eR32Uint;
				auto image_damage = context->m_device->createImageUnique(image_damage_info);

				vk::MemoryRequirements damage_memory_request = context->m_device->getImageMemoryRequirements(image_damage.get());
				vk::MemoryAllocateInfo damage_memory_alloc_info;
				damage_memory_alloc_info.allocationSize = damage_memory_request.size;
				damage_memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_device.getGPU(), damage_memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
				auto image_damage_memory = context->m_device->allocateMemoryUnique(damage_memory_alloc_info);
				context->m_device->bindImageMemory(image_damage.get(), image_damage_memory.get(), 0);

				vk::ImageCreateInfo sub_image_info = image_info;
				sub_image_info.extent.width = m_map_info_cpu.m_descriptor[1].m_cell_num.x;
				sub_image_info.extent.height = m_map_info_cpu.m_descriptor[1].m_cell_num.y;
				auto subimage = context->m_device->createImageUnique(sub_image_info);
				auto sub_memory_request = context->m_device->getImageMemoryRequirements(subimage.get());
				vk::MemoryAllocateInfo sub_memory_alloc_info;
				sub_memory_alloc_info.allocationSize = sub_memory_request.size;
				sub_memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_device.getGPU(), sub_memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
				auto subimage_memory = context->m_device->allocateMemoryUnique(sub_memory_alloc_info);
				context->m_device->bindImageMemory(subimage.get(), subimage_memory.get(), 0);

				vk::ImageSubresourceRange subresourceRange;
				subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				subresourceRange.baseArrayLayer = 0;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.layerCount = 1;
				subresourceRange.levelCount = 1;
				vk::ImageViewCreateInfo view_info;
				view_info.viewType = vk::ImageViewType::e2D;
				view_info.components.r = vk::ComponentSwizzle::eR;
				view_info.components.g = vk::ComponentSwizzle::eG;
				view_info.components.b = vk::ComponentSwizzle::eB;
				view_info.components.a = vk::ComponentSwizzle::eA;
				view_info.flags = vk::ImageViewCreateFlags();
				view_info.format = image_info.format;
				view_info.image = image.get();
				view_info.subresourceRange = subresourceRange;
				auto image_view = context->m_device->createImageViewUnique(view_info);

				view_info.image = subimage.get();
				auto subimage_view = context->m_device->createImageViewUnique(view_info);

				vk::ImageViewCreateInfo damage_view_info = vk::ImageViewCreateInfo(view_info)
					.setFormat(vk::Format::eR32Uint)
					.setImage(image_damage.get());
				auto image_damage_view = context->m_device->createImageViewUnique(damage_view_info);

				{

					vk::ImageMemoryBarrier to_transfer;
					to_transfer.image = image.get();
					to_transfer.oldLayout = vk::ImageLayout::eUndefined;
					to_transfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
					to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_transfer.subresourceRange = subresourceRange;
					vk::ImageMemoryBarrier to_transfer_sub = to_transfer;
					to_transfer_sub.image = subimage.get();
					vk::ImageMemoryBarrier to_transfer_damage = to_transfer;
					to_transfer_damage.image = image_damage.get();

					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_transfer, to_transfer_sub, to_transfer_damage });
				}

				{
					auto& map_desc = m_map_info_cpu.m_descriptor[1];
					m_maze.generate(map_desc.m_cell_num.x, map_desc.m_cell_num.y);
					auto submap = m_maze.makeMapData();
					std::vector<uint8_t> map(m_map_info_cpu.m_descriptor[0].m_cell_num.x* m_map_info_cpu.m_descriptor[0].m_cell_num.y);
					auto xy = m_map_info_cpu.m_descriptor[0].m_cell_num;
					for (size_t y = 0; y < xy.y; y++)
					{
						for (size_t x = 0; x < xy.x; x++)
						{
							auto i = y / m_map_info_cpu.m_subcell.y*m_map_info_cpu.m_descriptor[1].m_cell_num.x + x / m_map_info_cpu.m_subcell.x;
							auto m = submap[i];
							map[y*xy.x + x] = m;
						}
					}

					vk::ImageSubresourceLayers l;
					l.setAspectMask(vk::ImageAspectFlagBits::eColor);
					l.setBaseArrayLayer(0);
					l.setLayerCount(1);
					l.setMipLevel(0);
					{
						btr::BufferMemoryDescriptor desc;
						desc.size = context->m_device->getImageMemoryRequirements(image.get()).size;
						desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
						auto staging = context->m_staging_memory.allocateMemory(desc);
						memcpy_s(staging.getMappedPtr(), desc.size, map.data(), vector_sizeof(map));
						vk::BufferImageCopy copy;
						copy.setBufferOffset(staging.getBufferInfo().offset);
						copy.setImageSubresource(l);
						copy.setImageExtent(image_info.extent);

						cmd.copyBufferToImage(staging.getBufferInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copy);
					}
					{
						btr::BufferMemoryDescriptor desc;
						desc.size = context->m_device->getImageMemoryRequirements(subimage.get()).size;
						desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
						auto staging = context->m_staging_memory.allocateMemory(desc);

						memcpy_s(staging.getMappedPtr(), desc.size, submap.data(), vector_sizeof(submap));
						vk::BufferImageCopy copy;
						copy.setBufferOffset(staging.getBufferInfo().offset);
						copy.setImageSubresource(l);
						copy.setImageExtent(sub_image_info.extent);

						cmd.copyBufferToImage(staging.getBufferInfo().buffer, subimage.get(), vk::ImageLayout::eTransferDstOptimal, copy);

					}

					cmd.clearColorImage(image_damage.get(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(std::array<unsigned, 4>{}), subresourceRange);
				}

				{

					vk::ImageMemoryBarrier to_shader_read;
					to_shader_read.dstQueueFamilyIndex = context->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
					to_shader_read.image = image.get();
					to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
					to_shader_read.newLayout = vk::ImageLayout::eGeneral;
					to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
					to_shader_read.subresourceRange = subresourceRange;

					vk::ImageMemoryBarrier to_shader_read_sub = to_shader_read;
					to_shader_read_sub.image = subimage.get();

					vk::ImageMemoryBarrier to_compute_damege = to_shader_read;
					to_compute_damege.image = image_damage.get();
					to_compute_damege.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;

					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read,to_shader_read_sub, to_compute_damege });
				}
				m_map_image = std::move(image);
				m_map_image_view = std::move(image_view);
				m_map_image_memory = std::move(image_memory);

				m_submap_image = std::move(subimage);
				m_submap_image_view = std::move(subimage_view);
				m_submap_image_memory = std::move(subimage_memory);

				m_map_damae_image = std::move(image_damage);
				m_map_damae_image_view = std::move(image_damage_view);
				m_map_damae_image_memory = std::move(image_damage_memory);
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(MapInfo);
				m_map_info = context->m_uniform_memory.allocateMemory(desc);

				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				*staging.getMappedPtr<MapInfo>() = m_map_info_cpu;

				vk::BufferCopy vertex_copy;
				vertex_copy.setSize(desc.size);
				vertex_copy.setSrcOffset(staging.getBufferInfo().offset);
				vertex_copy.setDstOffset(m_map_info.getBufferInfo().offset);
				cmd.copyBuffer(staging.getBufferInfo().buffer, m_map_info.getBufferInfo().buffer, vertex_copy);

				auto barrier = m_map_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
			}
			{
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(SceneData);
				m_scene_data = context->m_uniform_memory.allocateMemory(desc);
			}

		}

		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		bindings[DESCRIPTOR_SET_LAYOUT_MAP] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setBinding(3),
		};
		bindings[DESCRIPTOR_SET_LAYOUT_SCENE] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
		};
		for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = context->m_device->createDescriptorSetLayoutUnique(descriptor_set_layout_info);
		}

		// descriptor set
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_MAP].get(),
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SCENE].get(),
			};
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = context->m_descriptor_pool.get();
			alloc_info.descriptorSetCount = array_length(layouts);
			alloc_info.pSetLayouts = layouts;
			auto descriptor_set = context->m_device->allocateDescriptorSetsUnique(alloc_info);
			std::copy(std::make_move_iterator(descriptor_set.begin()), std::make_move_iterator(descriptor_set.end()), m_descriptor_set.begin());


			{
				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_map_info.getBufferInfo(),
				};
				std::vector<vk::DescriptorImageInfo> images = {
					vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_map_image_view.get()),
					vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_submap_image_view.get()),
					vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_map_damae_image_view.get()),
				};
				std::vector<vk::WriteDescriptorSet> write_desc =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(uniforms.size())
					.setPBufferInfo(uniforms.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_MAP].get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(images.size())
					.setPImageInfo(images.data())
					.setDstBinding(1)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_MAP].get()),
				};
				context->m_device->updateDescriptorSets(write_desc, {});

			}
			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_scene_data.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_SCENE].get()),
			};
			context->m_device->updateDescriptorSets(write_desc, {});

		}

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}
			shader_info[] =
			{
				{ "FloorUpdateDamage.comp.spv",vk::ShaderStageFlagBits::eCompute },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
				m_shader_info[i].setModule(m_shader_module[i].get());
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				getDescriptorSetLayout(DESCRIPTOR_SET_LAYOUT_MAP),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_UPDATE_MAP] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(m_shader_info[SHADER_COMPUTE_MAP_UPDATE])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_UPDATE_MAP].get()),
			};
			auto pipelines = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PIPELINE_COMPUTE_UPDATE_MAP] = std::move(pipelines[0]);

		}

	}

	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& executer)
	{
		auto cmd = executer->m_cmd_pool->allocCmdOnetime(0);
		SceneData data;
		data.m_deltatime = sGlobal::Order().getDeltaTime();
		data.m_totaltime = sGlobal::Order().getTotalTime();

		btr::BufferMemoryDescriptor desc;
		desc.size = sizeof(SceneData);
		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = executer->m_staging_memory.allocateMemory(desc);

		*staging.getMappedPtr<SceneData>() = data;

		vk::BufferCopy copy;
		copy.setSize(desc.size);
		copy.setSrcOffset(staging.getBufferInfo().offset);
		copy.setDstOffset(m_scene_data.getBufferInfo().offset);
		cmd.copyBuffer(staging.getBufferInfo().buffer, m_scene_data.getBufferInfo().buffer, copy);

		vk::ImageMemoryBarrier to_read;
		to_read.oldLayout = vk::ImageLayout::eGeneral;
		to_read.newLayout = vk::ImageLayout::eGeneral;
		to_read.image = m_map_damae_image.get();
		to_read.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		to_read.subresourceRange.baseArrayLayer = 0;
		to_read.subresourceRange.baseMipLevel = 0;
		to_read.subresourceRange.layerCount = 1;
		to_read.subresourceRange.levelCount = 1;
		to_read.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		to_read.srcAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, to_read);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_UPDATE_MAP].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE_MAP].get(), 0, m_descriptor_set[DESCRIPTOR_SET_MAP].get(), {});
		auto groups = app::calcDipatchGroups(glm::uvec3(m_map_info_cpu.m_descriptor[0].m_cell_num, 1), glm::uvec3(32, 32, 1));
		cmd.dispatch(groups.x, groups.y, groups.z);

		cmd.end();
		return cmd;
	}

	glm::ivec2 calcMapIndex(const glm::vec4& p)
	{
		auto& map_desc = m_map_info_cpu.m_descriptor[0];
		glm::ivec2 map_index(glm::vec2(p.xz) / map_desc.m_cell_size);
		assert(glm::all(glm::greaterThanEqual(map_index, glm::ivec2(0))) && glm::all(glm::lessThan(map_index, glm::ivec2(map_desc.m_cell_num))));
		return map_index;

	}

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }

};

