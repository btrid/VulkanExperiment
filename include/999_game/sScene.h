#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/loader.h>
#include <btrlib/cCamera.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>
#include <999_game/MazeGenerator.h>
#include <999_game/Geometry.h>

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

struct sScene : public Singleton<sScene>
{

	enum 
	{
		SHADER_VERTEX_FLOOR,
		SHADER_GEOMETRY_FLOOR,
		SHADER_FRAGMENT_FLOOR,
		SHADER_VERTEX_FLOOR_EX,
		SHADER_FRAGMENT_FLOOR_EX,
		SHADER_COMPUTE_MAP_UPDATE,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MAP,
		DESCRIPTOR_SET_LAYOUT_CAMERA,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_MAP,
		DESCRIPTOR_SET_CAMERA,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_DRAW_FLOOR,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_FLOOR,
		PIPELINE_DRAW_FLOOR_EX,
		PIPELINE_COMPUTE_MAP,
		PIPELINE_NUM,
	};
	btr::UpdateBuffer<CameraGPU> m_camera;
	MapInfo m_map_info_cpu;

	MazeGenerator m_maze;
	Geometry m_maze_geometry;

	vk::Image m_map_image;
	vk::ImageView m_map_image_view;
	vk::DeviceMemory m_map_image_memory;
	vk::Image m_submap_image;
	vk::ImageView m_submap_image_view;
	vk::DeviceMemory m_submap_image_memory;
	vk::Image m_map_damae_image;
	vk::ImageView m_map_damae_image_view;
	vk::DeviceMemory m_map_damae_image_memory;
	btr::AllocatedMemory m_map_info;

	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::Pipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::DescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	void setup(std::shared_ptr<btr::Loader>& loader)
	{
		m_map_info_cpu.m_subcell = glm::uvec2(4, 4);
		m_map_info_cpu.m_descriptor[1].m_cell_size = glm::vec2(50.f, 50.f);
		m_map_info_cpu.m_descriptor[1].m_cell_num = glm::vec2(15, 15);
		m_map_info_cpu.m_descriptor[0].m_cell_size = m_map_info_cpu.m_descriptor[1].m_cell_size/glm::vec2(m_map_info_cpu.m_subcell);
		m_map_info_cpu.m_descriptor[0].m_cell_num = m_map_info_cpu.m_descriptor[1].m_cell_num*m_map_info_cpu.m_subcell;

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "FloorRender.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "FloorRender.geom.spv",vk::ShaderStageFlagBits::eGeometry },
				{ "FloorRender.frag.spv",vk::ShaderStageFlagBits::eFragment },
				{ "FloorRenderEx.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "FloorRenderEx.frag.spv",vk::ShaderStageFlagBits::eFragment },
				{ "MapUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_info[i].name));
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		{
			auto& map_desc = m_map_info_cpu.m_descriptor[1];
			m_maze.generate(map_desc.m_cell_num.x, map_desc.m_cell_num.y);
			auto geometry = m_maze.makeGeometry(glm::vec3(map_desc.m_cell_size.x, 1.f, map_desc.m_cell_size.y));
			Geometry::OptimaizeDuplicateVertexDescriptor opti_desc;

			std::vector<vk::VertexInputAttributeDescription> vertex_attr(1);
			vertex_attr[0].setOffset(0);
			vertex_attr[0].setBinding(0);
			vertex_attr[0].setFormat(vk::Format::eR32G32B32Sfloat);
			vertex_attr[0].setLocation(0);
			std::vector<vk::VertexInputBindingDescription> vertex_binding(1);
			vertex_binding[0].setBinding(0);
			vertex_binding[0].setInputRate(vk::VertexInputRate::eVertex);
			vertex_binding[0].setStride(sizeof(glm::vec3));
			m_maze_geometry = Geometry::MakeGeometry(
				loader,
				std::get<0>(geometry).data(),
				vector_sizeof(std::get<0>(geometry)),
				std::get<1>(geometry).data(),
				vector_sizeof(std::get<1>(geometry)),
				vk::IndexType::eUint32,
				vertex_attr,
				vertex_binding
			);

			{
				m_map_info_cpu.m_descriptor[0].m_cell_num;
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
				auto image = loader->m_device->createImage(image_info);

				vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(image);
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
				auto image_memory = loader->m_device->allocateMemory(memory_alloc_info);
				loader->m_device->bindImageMemory(image, image_memory, 0);

				vk::ImageCreateInfo image_damage_info = image_info;
				image_damage_info.format = vk::Format::eR32Uint;
				auto image_damage = loader->m_device->createImage(image_damage_info);

				vk::MemoryRequirements damage_memory_request = loader->m_device->getImageMemoryRequirements(image_damage);
				vk::MemoryAllocateInfo damage_memory_alloc_info;
				damage_memory_alloc_info.allocationSize = damage_memory_request.size;
				damage_memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), damage_memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
				auto image_damage_memory = loader->m_device->allocateMemory(damage_memory_alloc_info);
				loader->m_device->bindImageMemory(image_damage, image_damage_memory, 0);

				vk::ImageCreateInfo sub_image_info = image_info;
				sub_image_info.extent.width = m_map_info_cpu.m_descriptor[1].m_cell_num.x;
				sub_image_info.extent.height = m_map_info_cpu.m_descriptor[1].m_cell_num.y;
				auto subimage = loader->m_device->createImage(sub_image_info);
				auto sub_memory_request = loader->m_device->getImageMemoryRequirements(subimage);
				vk::MemoryAllocateInfo sub_memory_alloc_info;
				sub_memory_alloc_info.allocationSize = sub_memory_request.size;
				sub_memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), sub_memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
				auto subimage_memory = loader->m_device->allocateMemory(sub_memory_alloc_info);
				loader->m_device->bindImageMemory(subimage, subimage_memory, 0);

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
				view_info.image = image;
				view_info.subresourceRange = subresourceRange;
				auto image_view = loader->m_device->createImageView(view_info);

				view_info.image = subimage;
				auto subimage_view = loader->m_device->createImageView(view_info);

				vk::ImageViewCreateInfo damage_view_info = vk::ImageViewCreateInfo(view_info)
					.setFormat(vk::Format::eR32Uint)
					.setImage(image_damage);
				auto image_damage_view = loader->m_device->createImageView(damage_view_info);

				{

					vk::ImageMemoryBarrier to_transfer;
					to_transfer.image = image;
					to_transfer.oldLayout = vk::ImageLayout::eUndefined;
					to_transfer.newLayout = vk::ImageLayout::eTransferDstOptimal;
					to_transfer.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_transfer.subresourceRange = subresourceRange;
					vk::ImageMemoryBarrier to_transfer_sub = to_transfer;
					to_transfer_sub.image = subimage;
					vk::ImageMemoryBarrier to_transfer_damage = to_transfer;
					to_transfer_damage.image = image_damage;

					loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_transfer, to_transfer_sub, to_transfer_damage });
				}

				{
					auto submap = m_maze.makeMapData();
					decltype(submap) map(m_map_info_cpu.m_descriptor[0].m_cell_num.x* m_map_info_cpu.m_descriptor[0].m_cell_num.y);
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
						btr::BufferMemory::Descriptor desc;
						desc.size = loader->m_device->getImageMemoryRequirements(image).size;
						desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
						auto staging = loader->m_staging_memory.allocateMemory(desc);
						memcpy(staging.getMappedPtr(), map.data(), desc.size);
						vk::BufferImageCopy copy;
						copy.setBufferOffset(staging.getBufferInfo().offset);
						copy.setImageSubresource(l);
						copy.setImageExtent(image_info.extent);

						loader->m_cmd.copyBufferToImage(staging.getBufferInfo().buffer, image, vk::ImageLayout::eTransferDstOptimal, copy);
					}
					{
						btr::BufferMemory::Descriptor desc;
						desc.size = loader->m_device->getImageMemoryRequirements(subimage).size;
						desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
						auto staging = loader->m_staging_memory.allocateMemory(desc);

						memcpy(staging.getMappedPtr(), submap.data(), desc.size);
						vk::BufferImageCopy copy;
						copy.setBufferOffset(staging.getBufferInfo().offset);
						copy.setImageSubresource(l);
						copy.setImageExtent(sub_image_info.extent);

						loader->m_cmd.copyBufferToImage(staging.getBufferInfo().buffer, subimage, vk::ImageLayout::eTransferDstOptimal, copy);

					}

					loader->m_cmd.clearColorImage(image_damage, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(std::array<unsigned, 4>{}), subresourceRange);
				}

				{

					vk::ImageMemoryBarrier to_shader_read;
					to_shader_read.dstQueueFamilyIndex = loader->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
					to_shader_read.image = image;
					to_shader_read.oldLayout = vk::ImageLayout::eTransferDstOptimal;
					to_shader_read.newLayout = vk::ImageLayout::eGeneral;
					to_shader_read.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_shader_read.dstAccessMask = vk::AccessFlagBits::eShaderRead;
					to_shader_read.subresourceRange = subresourceRange;

					vk::ImageMemoryBarrier to_shader_read_sub = to_shader_read;
					to_shader_read_sub.image = subimage;

					vk::ImageMemoryBarrier to_compute_damege = to_shader_read;
					to_compute_damege.image = image_damage;
					to_compute_damege.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;

					loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read,to_shader_read_sub, to_compute_damege });
				}

				m_map_image = image;
				m_map_image_view = image_view;
				m_map_image_memory = image_memory;

				m_submap_image = subimage;
				m_submap_image_view = subimage_view;
				m_submap_image_memory = subimage_memory;

				m_map_damae_image = image_damage;
				m_map_damae_image_view = image_damage_view;
				m_map_damae_image_memory = image_damage_memory;
			}


			btr::BufferMemory::Descriptor desc;
			desc.size = sizeof(MapInfo);
			m_map_info = loader->m_uniform_memory.allocateMemory(desc);

			desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = loader->m_staging_memory.allocateMemory(desc);
			*staging.getMappedPtr<MapInfo>() = m_map_info_cpu;

			vk::BufferCopy vertex_copy;
			vertex_copy.setSize(desc.size);
			vertex_copy.setSrcOffset(staging.getBufferInfo().offset);
			vertex_copy.setDstOffset(m_map_info.getBufferInfo().offset);
			loader->m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_map_info.getBufferInfo().buffer, vertex_copy);

			auto barrier = m_map_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			loader->m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
		}

		btr::UpdateBufferDescriptor update_desc;
		update_desc.device_memory = loader->m_uniform_memory;
		update_desc.staging_memory = loader->m_staging_memory;
		update_desc.frame_max = sGlobal::FRAME_MAX;
		m_camera.setup(update_desc);

		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		bindings[DESCRIPTOR_SET_LAYOUT_MAP] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageImage)
			.setBinding(3),
		};
		bindings[DESCRIPTOR_SET_LAYOUT_CAMERA] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex| vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
		};
		for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = loader->m_device->createDescriptorSetLayout(descriptor_set_layout_info);
		}

		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = loader->m_descriptor_pool;
		alloc_info.descriptorSetCount = m_descriptor_set_layout.size();
		alloc_info.pSetLayouts = m_descriptor_set_layout.data();
		auto descriptor_set = loader->m_device->allocateDescriptorSets(alloc_info);
		std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_map_info.getBufferInfo(),
			};
			std::vector<vk::DescriptorImageInfo> images = {
				vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_map_image_view),
				vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_submap_image_view),
				vk::DescriptorImageInfo().setImageLayout(vk::ImageLayout::eGeneral).setImageView(m_map_damae_image_view),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_MAP]),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(images.size())
				.setPImageInfo(images.data())
				.setDstBinding(1)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_MAP]),
			};
			loader->m_device->updateDescriptorSets(write_desc, {});
		}
		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_camera.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LAYOUT_CAMERA]),
			};
			loader->m_device->updateDescriptorSets(write_desc, {});
		}

		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_CAMERA],
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_MAP],
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR] = loader->m_device->createPipelineLayout(pipeline_layout_info);
		}

		vk::Extent3D size;
		size.setWidth(640);
		size.setHeight(480);
		size.setDepth(1);
		// pipeline
		{
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(m_shader_info[SHADER_COMPUTE_MAP_UPDATE])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR]),
			};
			auto pipelines = loader->m_device->createComputePipelines(loader->m_cache, compute_pipeline_info);
			m_pipeline[PIPELINE_COMPUTE_MAP] = pipelines[0];

		}
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList),
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount((uint32_t)scissor.size());
			viewportInfo.setPScissors(scissor.data());

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info[2];
			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(glm::vec3))
			};

			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(0),
			};

			vertex_input_info[0].setVertexBindingDescriptionCount(vertex_input_binding.size());
			vertex_input_info[0].setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info[0].setVertexAttributeDescriptionCount(vertex_input_attribute.size());
			vertex_input_info[0].setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(3)
				.setPStages(&m_shader_info.data()[SHADER_VERTEX_FLOOR])
				.setPVertexInputState(&vertex_input_info[0])
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR])
				.setRenderPass(loader->m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(2)
				.setPStages(&m_shader_info.data()[SHADER_VERTEX_FLOOR_EX])
				.setPVertexInputState(&vertex_input_info[1])
				.setPInputAssemblyState(&assembly_info[1])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR])
				.setRenderPass(loader->m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = loader->m_device->createGraphicsPipelines(loader->m_cache, graphics_pipeline_info);
			std::copy(pipelines.begin(), pipelines.end(), m_pipeline.begin());

		}
	}

	void execute(std::shared_ptr<btr::Executer>& executer)
	{
		vk::CommandBuffer cmd = executer->m_cmd;

		std::vector<vk::BufferMemoryBarrier> to_transfer = {
			m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		auto* camera = cCamera::sCamera::Order().getCameraList()[0];
		CameraGPU camera_GPU;
		camera_GPU.setup(*camera);
		m_camera.subupdate(camera_GPU);
		m_camera.update(cmd);

		std::vector<vk::BufferMemoryBarrier> to_draw_barrier = {
			m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_draw_barrier }, {});


		{
//			auto to_read = m_soldier_emit_gpu.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
			vk::ImageMemoryBarrier to_read;
			to_read.oldLayout = vk::ImageLayout::eGeneral;
			to_read.newLayout = vk::ImageLayout::eGeneral;
			to_read.image = m_map_damae_image;
			to_read.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			to_read.subresourceRange.baseArrayLayer = 0;
			to_read.subresourceRange.baseMipLevel = 0;
			to_read.subresourceRange.layerCount = 1;
			to_read.subresourceRange.levelCount = 1;
			to_read.dstAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
			to_read.srcAccessMask = vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, to_read);


			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_MAP]);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR], 0, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_CAMERA], {});
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR], 1, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_MAP], {});
						auto groups = app::calcDipatchGroups(glm::uvec3(m_map_info_cpu.m_descriptor[0].m_cell_num, 1), glm::uvec3(32, 32, 1));
			cmd.dispatch(groups.x, groups.y, groups.z);

		}



	}
	void draw(vk::CommandBuffer cmd)
	{
// 		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_FLOOR]);
// 		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR], 0, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_CAMERA], {});
// 		cmd.bindVertexBuffers(0, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().buffer }, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().offset });
// 		cmd.bindIndexBuffer(m_maze_geometry.m_resource->m_index.getBufferInfo().buffer, m_maze_geometry.m_resource->m_index.getBufferInfo().offset, m_maze_geometry.m_resource->m_index_type);
//		cmd.drawIndexedIndirect(m_maze_geometry.m_resource->m_indirect.getBufferInfo().buffer, m_maze_geometry.m_resource->m_indirect.getBufferInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_FLOOR_EX]);
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR], 0, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_CAMERA], {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR], 1, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_MAP], {});
		cmd.draw(4, 1, 0, 0);
	}

	glm::ivec2 calcMapIndex(const glm::vec4& p)
	{
		auto& map_desc = m_map_info_cpu.m_descriptor[0];
		glm::vec2 cell_p = glm::mod(glm::vec2(p.xz), map_desc.m_cell_size);
		glm::ivec2 map_index(glm::vec2(p.xz) / map_desc.m_cell_size);
		assert(glm::all(glm::greaterThanEqual(map_index, glm::ivec2(0))) && glm::all(glm::lessThan(map_index, glm::ivec2(map_desc.m_cell_num))));
		return map_index;

	}

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_pipeline_layout[layout]; }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor]; }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i]; }

};