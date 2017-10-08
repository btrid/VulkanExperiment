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

	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;
	std::vector<vk::UniqueCommandBuffer> m_cmd;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	VoxelPipeline m_voxelize_pipeline;

	void setup(std::shared_ptr<btr::Context>& context)
	{
		m_map_info_cpu.m_subcell = glm::uvec2(4, 4);
		m_map_info_cpu.m_descriptor[1].m_cell_size = glm::vec2(50.f, 50.f);
		m_map_info_cpu.m_descriptor[1].m_cell_num = glm::vec2(15, 15);
		m_map_info_cpu.m_descriptor[0].m_cell_size = m_map_info_cpu.m_descriptor[1].m_cell_size/glm::vec2(m_map_info_cpu.m_subcell);
		m_map_info_cpu.m_descriptor[0].m_cell_num = m_map_info_cpu.m_descriptor[1].m_cell_num*m_map_info_cpu.m_subcell;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			// レンダーパス
			{
				// sub pass
				std::vector<vk::AttachmentReference> color_ref =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(1);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount((uint32_t)color_ref.size());
				subpass.setPColorAttachments(color_ref.data());
				subpass.setPDepthStencilAttachment(&depth_ref);

				std::vector<vk::AttachmentDescription> attach_description = {
					// color1
					vk::AttachmentDescription()
					.setFormat(context->m_window->getSwapchain().m_surface_format.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					vk::AttachmentDescription()
					.setFormat(vk::Format::eD32Sfloat)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
					.setAttachmentCount(attach_description.size())
					.setPAttachments(attach_description.data())
					.setSubpassCount(1)
					.setPSubpasses(&subpass);

				m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
			}

			m_framebuffer.resize(context->m_window->getSwapchain().getBackbufferNum());
			{
				std::array<vk::ImageView, 2> view;

				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass.get());
				framebuffer_info.setAttachmentCount((uint32_t)view.size());
				framebuffer_info.setPAttachments(view.data());
				framebuffer_info.setWidth(context->m_window->getClientSize().x);
				framebuffer_info.setHeight(context->m_window->getClientSize().y);
				framebuffer_info.setLayers(1);

				for (size_t i = 0; i < m_framebuffer.size(); i++) {
					view[0] = context->m_window->getSwapchain().m_backbuffer[i].m_view;
					view[1] = context->m_window->getSwapchain().m_depth.m_view;
					m_framebuffer[i] = context->m_device->createFramebufferUnique(framebuffer_info);
				}
			}

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
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
				m_shader_info[i].setModule(m_shader_module[i].get());
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
// 			m_maze_geometry = Geometry::MakeGeometry(
// 				loader,
// 				std::get<0>(geometry).data(),
// 				vector_sizeof(std::get<0>(geometry)),
// 				std::get<1>(geometry).data(),
// 				vector_sizeof(std::get<1>(geometry)),
// 				vk::IndexType::eUint32,
// 				vertex_attr,
// 				vertex_binding
//			);
			{
// 				vk::ImageCreateInfo image_info;
// 				image_info.imageType = vk::ImageType::e2D;
// 				image_info.format = vk::Format::eR32Sfloat;
// 				image_info.mipLevels = 1;
// 				image_info.arrayLayers = 1;
// 				image_info.samples = vk::SampleCountFlagBits::e1;
// 				image_info.tiling = vk::ImageTiling::eOptimal;
// 				image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
// 				image_info.sharingMode = vk::SharingMode::eExclusive;
// 				image_info.initialLayout = vk::ImageLayout::eUndefined;
// 				image_info.extent = { m_map_info_cpu.m_descriptor[0].m_cell_num.x, m_map_info_cpu.m_descriptor[0].m_cell_num.y, 1u };
// 				m_map_emission_image = loader->m_device->createImageUnique(image_info);
// 
// 				vk::MemoryRequirements memory_request = loader->m_device->getImageMemoryRequirements(m_map_emission_image.get());
// 				vk::MemoryAllocateInfo memory_alloc_info;
// 				memory_alloc_info.allocationSize = memory_request.size;
// 				memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
// 				m_map_emission_image_memory = loader->m_device->allocateMemoryUnique(memory_alloc_info);
// 				loader->m_device->bindImageMemory(m_map_emission_image.get(), m_map_emission_image_memory.get(), 0);
// 
// 
// 				vk::ImageSubresourceRange subresourceRange;
// 				subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
// 				subresourceRange.baseArrayLayer = 0;
// 				subresourceRange.baseMipLevel = 0;
// 				subresourceRange.layerCount = 1;
// 				subresourceRange.levelCount = 1;
// 				vk::ImageViewCreateInfo view_info;
// 				view_info.viewType = vk::ImageViewType::e2D;
// 				view_info.components.r = vk::ComponentSwizzle::eR;
// 				view_info.components.g = vk::ComponentSwizzle::eG;
// 				view_info.components.b = vk::ComponentSwizzle::eB;
// 				view_info.components.a = vk::ComponentSwizzle::eA;
// 				view_info.flags = vk::ImageViewCreateFlags();
// 				view_info.format = image_info.format;
// 				view_info.image = m_map_emission_image.get();
// 				view_info.subresourceRange = subresourceRange;
// 				m_map_emission_image_view = loader->m_device->createImageViewUnique(view_info);
// 
// // 				std::vector<float> emissive
// // 				auto xy = m_map_info_cpu.m_descriptor[0].m_cell_num;
// // 				for (size_t y = 0; y < xy.y; y++)
// // 				{
// // 					for (size_t x = 0; x < xy.x; x++)
// // 					{
// // 						auto i = y / m_map_info_cpu.m_subcell.y*m_map_info_cpu.m_descriptor[1].m_cell_num.x + x / m_map_info_cpu.m_subcell.x;
// // 						auto m = submap[i];
// // 						map[y*xy.x + x] = m;
// // 					}
// // 				}
// 
// 				vk::ImageSubresourceLayers l;
// 				l.setAspectMask(vk::ImageAspectFlagBits::eColor);
// 				l.setBaseArrayLayer(0);
// 				l.setLayerCount(1);
// 				l.setMipLevel(0);
// 				{
// 					btr::AllocatedMemory::Descriptor desc;
// 	//				desc.size = loader->m_device->getImageMemoryRequirements(image.get()).size;
// 					desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
// 					auto staging = loader->m_staging_memory.allocateMemory(desc);
// 	//				memcpy(staging.getMappedPtr(), map.data(), desc.size);
// 					vk::BufferImageCopy copy;
// 					copy.setBufferOffset(staging.getBufferInfo().offset);
// 					copy.setImageSubresource(l);
// 					copy.setImageExtent(image_info.extent);
// 
// 	//				cmd->copyBufferToImage(staging.getBufferInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copy);
// 				}
// 				{
// 					btr::AllocatedMemory::Descriptor desc;
// 	//				desc.size = loader->m_device->getImageMemoryRequirements(subimage.get()).size;
// 					desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
// 					auto staging = loader->m_staging_memory.allocateMemory(desc);
// 
// 	//				memcpy(staging.getMappedPtr(), submap.data(), desc.size);
// 					vk::BufferImageCopy copy;
// 					copy.setBufferOffset(staging.getBufferInfo().offset);
// 					copy.setImageSubresource(l);
// //					copy.setImageExtent(sub_image_info.extent);
// 
// 	//				cmd->copyBufferToImage(staging.getBufferInfo().buffer, subimage.get(), vk::ImageLayout::eTransferDstOptimal, copy);
// 
// 				}
			}
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

					cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_transfer, to_transfer_sub, to_transfer_damage });
				}

				{
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
						btr::AllocatedMemory::Descriptor desc;
						desc.size = context->m_device->getImageMemoryRequirements(image.get()).size;
						desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
						auto staging = context->m_staging_memory.allocateMemory(desc);
						memcpy_s(staging.getMappedPtr(), desc.size, map.data(), vector_sizeof(map));
						vk::BufferImageCopy copy;
						copy.setBufferOffset(staging.getBufferInfo().offset);
						copy.setImageSubresource(l);
						copy.setImageExtent(image_info.extent);

						cmd->copyBufferToImage(staging.getBufferInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copy);
					}
					{
						btr::AllocatedMemory::Descriptor desc;
						desc.size = context->m_device->getImageMemoryRequirements(subimage.get()).size;
						desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
						auto staging = context->m_staging_memory.allocateMemory(desc);

						memcpy_s(staging.getMappedPtr(), desc.size, submap.data(), vector_sizeof(submap));
						vk::BufferImageCopy copy;
						copy.setBufferOffset(staging.getBufferInfo().offset);
						copy.setImageSubresource(l);
						copy.setImageExtent(sub_image_info.extent);

						cmd->copyBufferToImage(staging.getBufferInfo().buffer, subimage.get(), vk::ImageLayout::eTransferDstOptimal, copy);

					}
					
					cmd->clearColorImage(image_damage.get(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(std::array<unsigned, 4>{}), subresourceRange);
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

					cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read,to_shader_read_sub, to_compute_damege });
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
				btr::AllocatedMemory::Descriptor desc;
				desc.size = sizeof(MapInfo);
				m_map_info = context->m_uniform_memory.allocateMemory(desc);

				desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging = context->m_staging_memory.allocateMemory(desc);
				*staging.getMappedPtr<MapInfo>() = m_map_info_cpu;

				vk::BufferCopy vertex_copy;
				vertex_copy.setSize(desc.size);
				vertex_copy.setSrcOffset(staging.getBufferInfo().offset);
				vertex_copy.setDstOffset(m_map_info.getBufferInfo().offset);
				cmd->copyBuffer(staging.getBufferInfo().buffer, m_map_info.getBufferInfo().buffer, vertex_copy);

				auto barrier = m_map_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});
			}
			{
				btr::AllocatedMemory::Descriptor desc;
				desc.size = sizeof(SceneData);
				m_scene_data = context->m_uniform_memory.allocateMemory(desc);
			}

		}

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
		bindings[DESCRIPTOR_SET_LAYOUT_SCENE] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
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

		{
			vk::DescriptorSetLayout layouts[] = {
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_MAP].get(),
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_SCENE].get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get()),
			};
			auto pipelines = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PIPELINE_COMPUTE_MAP] = std::move(pipelines[0]);

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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get())
				.setRenderPass(m_render_pass.get())
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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			std::copy(std::make_move_iterator(pipelines.begin()), std::make_move_iterator(pipelines.end()), m_pipeline.begin());

		}

		vk::CommandBufferAllocateInfo cmd_info;
		cmd_info.commandBufferCount = sGlobal::FRAME_MAX;
		cmd_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_info.level = vk::CommandBufferLevel::ePrimary;
		m_cmd = std::move(context->m_device->allocateCommandBuffersUnique(cmd_info));

		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		for (size_t i = 0; i < m_cmd.size(); i++)
		{
			auto& cmd = m_cmd[i];
			cmd->begin(begin_info);
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
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, to_read);

			cmd->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_MAP].get());
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 1, m_descriptor_set[DESCRIPTOR_SET_MAP].get(), {});
			auto groups = app::calcDipatchGroups(glm::uvec3(m_map_info_cpu.m_descriptor[0].m_cell_num, 1), glm::uvec3(32, 32, 1));
			cmd->dispatch(groups.x, groups.y, groups.z);

			{
				vk::RenderPassBeginInfo begin_render_Info = vk::RenderPassBeginInfo()
					.setRenderPass(m_render_pass.get())
					.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(context->m_window->getClientSize().x, context->m_window->getClientSize().y)))
					.setFramebuffer(m_framebuffer[i].get());
				cmd->beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

// 				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_FLOOR]);
// 				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR], 0, m_descriptor_set[DESCRIPTOR_SET_LAYOUT_CAMERA], {});
// 				cmd.bindVertexBuffers(0, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().buffer }, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().offset });
// 				cmd.bindIndexBuffer(m_maze_geometry.m_resource->m_index.getBufferInfo().buffer, m_maze_geometry.m_resource->m_index.getBufferInfo().offset, m_maze_geometry.m_resource->m_index_type);
// 				cmd.drawIndexedIndirect(m_maze_geometry.m_resource->m_indirect.getBufferInfo().buffer, m_maze_geometry.m_resource->m_indirect.getBufferInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

				cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DRAW_FLOOR_EX].get());
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 1, m_descriptor_set[DESCRIPTOR_SET_MAP].get(), {});
				cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR].get(), 2, m_descriptor_set[DESCRIPTOR_SET_SCENE].get(), {});
				cmd->draw(4, 1, 0, 0);

				cmd->endRenderPass();
			}
			cmd->end();
		}

		VoxelInfo info;
		info.u_area_min = vec4(0.f, 0.f, 0.f, 1.f);
		info.u_area_max = vec4(500.f, 20.f, 500.f, 1.f);
		info.u_cell_num = uvec4(128, 4, 128, 1);
		info.u_cell_size = (info.u_area_max - info.u_area_min) / vec4(info.u_cell_num);
		m_voxelize_pipeline.setup(context, info);

	}

	vk::CommandBuffer draw1(std::shared_ptr<btr::Context>& executer)
	{
		auto cmd = executer->m_cmd_pool->allocCmdOnetime(0);
		SceneData data;
		data.m_deltatime = sGlobal::Order().getDeltaTime();
		data.m_totaltime = sGlobal::Order().getTotalTime();

		btr::AllocatedMemory::Descriptor desc;
		desc.size = sizeof(SceneData);
		desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = executer->m_staging_memory.allocateMemory(desc);

		*staging.getMappedPtr<SceneData>() = data;

		vk::BufferCopy copy;
		copy.setSize(desc.size);
		copy.setSrcOffset(staging.getBufferInfo().offset);
		copy.setDstOffset(m_scene_data.getBufferInfo().offset);
		cmd.copyBuffer(staging.getBufferInfo().buffer, m_scene_data.getBufferInfo().buffer, copy);
		cmd.end();
		return cmd;
	}
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer)
	{
		return m_cmd[sGlobal::Order().getCurrentFrame()].get();
	}

	glm::ivec2 calcMapIndex(const glm::vec4& p)
	{
		auto& map_desc = m_map_info_cpu.m_descriptor[0];
		glm::ivec2 map_index(glm::vec2(p.xz) / map_desc.m_cell_size);
		assert(glm::all(glm::greaterThanEqual(map_index, glm::ivec2(0))) && glm::all(glm::lessThan(map_index, glm::ivec2(map_desc.m_cell_num))));
		return map_index;

	}

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_pipeline_layout[layout].get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }
	VoxelPipeline& getVoxel() { return m_voxelize_pipeline; }

};

