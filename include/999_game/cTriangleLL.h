#pragma once

#include <array>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCamera.h>

#include <btrlib/Context.h>

struct cTriangleLL
{
	struct BrickParam
	{
		glm::uvec4	m_cell_num;	//!< xyz:1d w:x*y*z 
		glm::uvec4	m_subcell_num;	//!< セル内のセルの数

		glm::uvec4 m_total_num;		//!< 1d

		glm::vec4 m_area_min;
		glm::vec4 m_area_max;

		// 事前計算
		glm::vec4 m_cell_size;
		glm::vec4 m_subcell_size;

	};
	struct test
	{
		cTriangleLL* m_parent;
		BrickParam	m_brick_info;
		std::array<vk::PipelineShaderStageCreateInfo, 2> m_draw_triangleLL_shader_info;
		vk::DescriptorPool m_descriptor_pool;
		vk::DescriptorSetLayout m_descriptor_set_layout;
		vk::DescriptorSet m_descriptor_set;
		vk::PipelineLayout m_pipeline_layout;
		vk::Pipeline m_pipeline;

		btr::BufferMemory m_vertex;
		btr::BufferMemory m_index;
		uint32_t m_index_count;
		void setup(btr::Context& loader, cTriangleLL* const t)
		{
			m_parent = t;
			m_brick_info = m_parent->m_brick_info;
			{
				// メモリ確保
				std::vector<glm::vec3> v;
				std::vector<glm::uvec3> i;
				std::tie(v, i) = Geometry::MakeBox(0.5f);
				m_index_count = i.size() * 3;
				{
					btr::BufferMemoryDescriptor desc;
					desc.size = vector_sizeof(v);
					m_vertex = loader.m_vertex_memory.allocateMemory(desc);

					desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
					auto staging = loader.m_staging_memory.allocateMemory(desc);
					memcpy(staging.getMappedPtr(), v.data(), desc.size);

					vk::BufferCopy copy;
					copy.setSize(desc.size);
					copy.setSrcOffset(staging.getBufferInfo().offset);
					copy.setDstOffset(m_vertex.getBufferInfo().offset);
					loader.m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_vertex.getBufferInfo().buffer, copy);

				}
				{
					btr::BufferMemoryDescriptor desc;
					desc.size = vector_sizeof(i);
					m_index = loader.m_vertex_memory.allocateMemory(desc);

					desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
					auto staging = loader.m_staging_memory.allocateMemory(desc);
					memcpy(staging.getMappedPtr(), i.data(), desc.size);

					vk::BufferCopy copy;
					copy.setSize(desc.size);
					copy.setSrcOffset(staging.getBufferInfo().offset);
					copy.setDstOffset(m_index.getBufferInfo().offset);
					loader.m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_index.getBufferInfo().buffer, copy);

				}

			}

			{
				struct ShaderDesc {
					const char* name;
					vk::ShaderStageFlagBits stage;
				} shader_desc[] =
				{
					{ "RenderBrick.vert.spv", vk::ShaderStageFlagBits::eVertex },
					{ "RenderBrick.frag.spv", vk::ShaderStageFlagBits::eFragment },
				};
				std::string path = btr::getResourceAppPath() + "shader\\binary\\";
				for (size_t i = 0; i < sizeof(shader_desc) / sizeof(shader_desc[0]); i++)
				{
					m_draw_triangleLL_shader_info[i].setModule(loadShader(loader.m_device.get(), path + shader_desc[i].name));
					m_draw_triangleLL_shader_info[i].setStage(shader_desc[i].stage);
					m_draw_triangleLL_shader_info[i].setPName("main");
				}
			}

			{
				std::vector<vk::DescriptorSetLayoutBinding> bindings =
				{
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setBinding(0),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setBinding(1),
				};
				auto descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
					.setBindingCount(bindings.size())
					.setPBindings(bindings.data());
				m_descriptor_set_layout = loader.m_device.createDescriptorSetLayout(descriptor_set_layout_info);

				m_descriptor_pool = createDescriptorPool(loader.m_device.get(), { bindings });

				vk::DescriptorSetAllocateInfo alloc_info;
				alloc_info.descriptorPool = m_descriptor_pool;
				alloc_info.descriptorSetCount = 1;
				alloc_info.pSetLayouts = &m_descriptor_set_layout;
				m_descriptor_set = loader.m_device.allocateDescriptorSets(alloc_info)[0];

			}

			{
				{
					std::vector<vk::DescriptorSetLayout> layouts = {
						m_descriptor_set_layout,
					};
					std::vector<vk::PushConstantRange> push_constants = {
						vk::PushConstantRange()
						.setStageFlags(vk::ShaderStageFlagBits::eVertex)
						.setSize(64),
					};
					vk::PipelineLayoutCreateInfo pipeline_layout_info;
					pipeline_layout_info.setSetLayoutCount(layouts.size());
					pipeline_layout_info.setPSetLayouts(layouts.data());
					pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
					pipeline_layout_info.setPPushConstantRanges(push_constants.data());
					m_pipeline_layout = loader.m_device.createPipelineLayout(pipeline_layout_info);
				}

				{

					std::vector<vk::DescriptorBufferInfo> uniforms = {
						m_parent->m_triangle_info.getBufferInfo(),
					};
					std::vector<vk::DescriptorBufferInfo> storages = {
						m_parent->m_triangleLL_head.getBufferInfo(),
					};
					std::vector<vk::WriteDescriptorSet> write_desc =
					{
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(uniforms.size())
						.setPBufferInfo(uniforms.data())
						.setDstBinding(0)
						.setDstSet(m_descriptor_set),
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(storages.size())
						.setPBufferInfo(storages.data())
						.setDstBinding(1)
						.setDstSet(m_descriptor_set),
					};
					loader.m_device.updateDescriptorSets(write_desc, {});
				}

			}
			// pipeline
			{
				// assembly
				auto assembly_info = vk::PipelineInputAssemblyStateCreateInfo();
				assembly_info.setPrimitiveRestartEnable(VK_FALSE);
				assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

				// viewport
				vk::Viewport viewport[1];
				viewport[0] = vk::Viewport(0.f, 0.f, 640.f, 480.f, 0.f, 1.f);
				std::vector<vk::Rect2D> scissor = {
					vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(viewport[0].width, viewport[0].height)),
				};
				vk::PipelineViewportStateCreateInfo viewportInfo;
				viewportInfo.setViewportCount(1);
				viewportInfo.setPViewports(viewport);
				viewportInfo.setScissorCount((uint32_t)scissor.size());
				viewportInfo.setPScissors(scissor.data());

				// ラスタライズ
				vk::PipelineRasterizationStateCreateInfo rasterization_info;
				rasterization_info.setPolygonMode(vk::PolygonMode::eLine);
				rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
				rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
				rasterization_info.setLineWidth(1.f);
				// サンプリング
				vk::PipelineMultisampleStateCreateInfo sample_info;
				sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

				// デプスステンシル
				vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
				depth_stencil_info.setDepthTestEnable(VK_FALSE);
				depth_stencil_info.setDepthWriteEnable(VK_FALSE);

				// ブレンド
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

				// vertex input
				std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
				{
					vk::VertexInputBindingDescription()
					.setBinding(0)
					.setInputRate(vk::VertexInputRate::eVertex)
					.setStride(sizeof(glm::vec3))
				};
				std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
				{
					// pos
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(0)
					.setFormat(vk::Format::eR32G32B32Sfloat)
					.setOffset(0),
				};
				vk::PipelineVertexInputStateCreateInfo vertex_input_info;
				vertex_input_info.setVertexBindingDescriptionCount(vertex_input_binding.size());
				vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
				vertex_input_info.setVertexAttributeDescriptionCount(vertex_input_attribute.size());
				vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

				std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
				{
					vk::GraphicsPipelineCreateInfo()
					.setStageCount(m_draw_triangleLL_shader_info.size())
					.setPStages(m_draw_triangleLL_shader_info.data())
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&assembly_info)
					.setPViewportState(&viewportInfo)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pipeline_layout)
					.setRenderPass(loader.m_render_pass)
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info),
				};
				m_pipeline = loader.m_device.createGraphicsPipelines(vk::PipelineCache(), graphics_pipeline_info)[0];
			}

		}

		void draw(vk::CommandBuffer cmd)
		{

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout, 0, m_descriptor_set, {});
			cmd.bindVertexBuffers(0, { m_vertex.getBufferInfo().buffer }, { m_vertex.getBufferInfo().offset });
			cmd.bindIndexBuffer(m_index.getBufferInfo().buffer, m_index.getBufferInfo().offset, vk::IndexType::eUint32);

			CameraGPU camera;
			camera.setup(*cCamera::sCamera::Order().getCameraList()[0]);
			auto pv = camera.u_projection * camera.u_view;
			cmd.pushConstants<glm::mat4>(m_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, pv);

			cmd.drawIndexed(m_index_count, m_brick_info.m_total_num.w + 1, 0, 0, 0);

		}
	};

	struct TriangleLL
	{
		std::int32_t next;
		std::int32_t drawID;
		std::int32_t instanceID;
		std::int32_t _p1;

		std::int32_t index[3];
		std::int32_t _p2;
	};

	struct TriangleProjection {
		glm::mat4 ortho[3];
		glm::mat4 view[3];
	};
	vk::RenderPass m_render_pass;
	vk::Framebuffer m_framebuffer;

	vk::DescriptorPool m_descriptor_pool;
	vk::DescriptorSetLayout m_descriptor_set_layout;
	vk::DescriptorSet m_descriptor_set;
	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;

	BrickParam	m_brick_info;
	btr::BufferMemory m_triangle_info;
	btr::BufferMemory m_triangleLL_head;
	btr::BufferMemory m_triangleLL;
	btr::BufferMemory m_triangleLL_count;
	btr::UpdateBuffer<TriangleProjection> m_triangle_projection;
	vk::Image m_brick_image;
	vk::ImageView m_brick_image_view;
	vk::DeviceMemory m_brick_image_memory;

	test m_test;
	enum {
		MAKE_TRIANGLELL_VERT,
		MAKE_TRIANGLELL_GEOM,
		MAKE_TRIANGLELL_FRAG,
		MAKE_TRIANGLELL_NUM,
	};
	std::array<vk::PipelineShaderStageCreateInfo, MAKE_TRIANGLELL_NUM> m_make_triangleLL_shader_info;

	void setup(btr::Context& loader)
	{
		{
			// メモリ確保
			{
				// info
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(BrickParam);
				m_triangle_info = loader.m_uniform_memory.allocateMemory(desc);

				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
				auto staging_buffer = loader.m_staging_memory.allocateMemory(desc);
				BrickParam& param = *staging_buffer.getMappedPtr<BrickParam>();

				param.m_cell_num = glm::uvec4(32, 4, 32, 32 * 4 * 32);
				param.m_subcell_num = glm::uvec4(4, 2, 4, 4 * 2 * 4);

				param.m_total_num = param.m_cell_num*param.m_subcell_num;
				param.m_area_min = glm::vec4(0.f);
				param.m_area_max = glm::vec4(100.f);
				param.m_cell_size = (param.m_area_max - param.m_area_min) / glm::vec4(param.m_cell_num);
				param.m_subcell_size = param.m_cell_size / glm::vec4(param.m_subcell_num);

				m_brick_info = param;

				vk::BufferCopy copy;
				copy.setSize(desc.size);
				copy.setSrcOffset(staging_buffer.getBufferInfo().offset);
				copy.setDstOffset(m_triangle_info.getBufferInfo().offset);
				loader.m_cmd.copyBuffer(staging_buffer.getBufferInfo().buffer, m_triangle_info.getBufferInfo().buffer, copy);
			}

			{
				// head
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t)*m_brick_info.m_total_num.w;
				m_triangleLL_head = loader.m_storage_memory.allocateMemory(desc);
			}
			{
				// link list
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(TriangleLL) * 1000 * 1000;
				m_triangleLL = loader.m_storage_memory.allocateMemory(desc);
			}
			{
				// triangle counter
				btr::BufferMemoryDescriptor desc;
				desc.size = sizeof(uint32_t);
				m_triangleLL_count = loader.m_storage_memory.allocateMemory(desc);
			}

			{
				btr::UpdateBufferDescriptor desc;
				desc.device_memory = loader.m_uniform_memory;
				desc.staging_memory = loader.m_staging_memory;
				desc.frame_max = sGlobal::FRAME_COUNT_MAX;
				m_triangle_projection.setup(desc);
			}
			{
				vk::ImageCreateInfo image_info;
				image_info.imageType = vk::ImageType::e3D;
				image_info.format = vk::Format::eR8G8B8A8Uint;
				image_info.mipLevels = 1;
				image_info.arrayLayers = 1;
				image_info.samples = vk::SampleCountFlagBits::e1;
				image_info.tiling = vk::ImageTiling::eOptimal;
				image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
				image_info.sharingMode = vk::SharingMode::eExclusive;
				image_info.initialLayout = vk::ImageLayout::eUndefined;
				image_info.extent = { m_brick_info.m_cell_num.x, m_brick_info.m_cell_num.y, m_brick_info.m_cell_num.z };
				image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
				m_brick_image = loader.m_device.createImage(image_info);

				vk::MemoryRequirements memory_request = loader.m_device.getImageMemoryRequirements(m_brick_image);
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader.m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_brick_image_memory = loader.m_device.allocateMemory(memory_alloc_info);
				loader.m_device.bindImageMemory(m_brick_image, m_brick_image_memory, 0);

				vk::ImageSubresourceRange subresourceRange;
				subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				subresourceRange.baseArrayLayer = 0;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.layerCount = 1;
				subresourceRange.levelCount = 1;

				vk::ImageViewCreateInfo view_info;
				view_info.viewType = vk::ImageViewType::e3D;
				view_info.components.r = vk::ComponentSwizzle::eR;
				view_info.components.g = vk::ComponentSwizzle::eG;
				view_info.components.b = vk::ComponentSwizzle::eB;
				view_info.components.a = vk::ComponentSwizzle::eA;
				view_info.flags = vk::ImageViewCreateFlags();
				view_info.format = image_info.format;
				view_info.image = m_brick_image;
				view_info.subresourceRange = subresourceRange;
				m_brick_image_view = loader.m_device.createImageView(view_info);

				{

					vk::ImageMemoryBarrier to_shader_read_barrier;
					to_shader_read_barrier.dstQueueFamilyIndex = loader.m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
					to_shader_read_barrier.image = m_brick_image;
					to_shader_read_barrier.oldLayout = vk::ImageLayout::eUndefined;
					to_shader_read_barrier.newLayout = vk::ImageLayout::eGeneral;
					to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
					to_shader_read_barrier.subresourceRange = subresourceRange;

					loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });

				}

			}
		}

		{
			struct ShaderDesc {
				const char* name;
				vk::ShaderStageFlagBits stage;
			} shader_desc[] =
			{
				{ "MakeTriangleLL.vert.spv", vk::ShaderStageFlagBits::eVertex },
				{ "MakeTriangleLL.geom.spv", vk::ShaderStageFlagBits::eGeometry },
				{ "MakeTriangleLL.frag.spv", vk::ShaderStageFlagBits::eFragment },
			};
			//			static_assert(array_length(shader_desc) == COMPUTE_NUM, "not equal shader num");
			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < sizeof(shader_desc) / sizeof(shader_desc[0]); i++)
			{
				m_make_triangleLL_shader_info[i].setModule(loadShader(loader.m_device.get(), path + shader_desc[i].name));
				m_make_triangleLL_shader_info[i].setStage(shader_desc[i].stage);
				m_make_triangleLL_shader_info[i].setPName("main");
			}
		}

		{
			std::vector<vk::DescriptorSetLayoutBinding> bindings =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setBinding(2),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setBinding(3),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setBinding(4),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setBinding(16),
			};
			auto descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings.size())
				.setPBindings(bindings.data());
			m_descriptor_set_layout = loader.m_device.createDescriptorSetLayout(descriptor_set_layout_info);

			m_descriptor_pool = createDescriptorPool(loader.m_device.get(), { bindings });

			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = m_descriptor_pool;
			alloc_info.descriptorSetCount = 1;
			alloc_info.pSetLayouts = &m_descriptor_set_layout;
			m_descriptor_set = loader.m_device.allocateDescriptorSets(alloc_info)[0];

		}

		{
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout,
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
				m_pipeline_layout = loader.m_device.createPipelineLayout(pipeline_layout_info);
			}

			{

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_triangle_info.getBufferInfo(),
					m_triangle_projection.getBufferInfo(),
				};
				std::vector<vk::DescriptorBufferInfo> storages = {
					m_triangleLL_head.getBufferInfo(),
					m_triangleLL.getBufferInfo(),
					m_triangleLL_count.getBufferInfo(),
				};
				std::vector<vk::DescriptorImageInfo> images = {
					vk::DescriptorImageInfo()
					.setImageView(m_brick_image_view)
					.setImageLayout(vk::ImageLayout::eGeneral),
				};
				std::vector<vk::WriteDescriptorSet> write_desc =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(uniforms.size())
					.setPBufferInfo(uniforms.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(storages.size())
					.setPBufferInfo(storages.data())
					.setDstBinding(2)
					.setDstSet(m_descriptor_set),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(images.size())
					.setPImageInfo(images.data())
					.setDstBinding(16)
					.setDstSet(m_descriptor_set),
				};
				loader.m_device.updateDescriptorSets(write_desc, {});
			}

		}
		{
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo();
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);
			m_render_pass = loader.m_device.createRenderPass(renderpass_info);


			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass);
			framebuffer_info.setHeight(128);
			framebuffer_info.setWidth(128);
			m_framebuffer = loader.m_device.createFramebuffer(framebuffer_info);
		}
		// pipeline
		{
			// assembly
			auto assembly_info = vk::PipelineInputAssemblyStateCreateInfo();
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport[3];
			viewport[0] = vk::Viewport(0.f, 0.f, (float)m_brick_info.m_total_num.z, (float)m_brick_info.m_total_num.y, 0.f, 1.f);
			viewport[1] = vk::Viewport(0.f, 0.f, (float)m_brick_info.m_total_num.x, (float)m_brick_info.m_total_num.z, 0.f, 1.f);
			viewport[2] = vk::Viewport(0.f, 0.f, (float)m_brick_info.m_total_num.x, (float)m_brick_info.m_total_num.y, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(viewport[0].width, viewport[0].height)),
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(viewport[1].width, viewport[1].height)),
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(viewport[2].width, viewport[2].height)),
			};
			vk::PipelineViewportStateCreateInfo viewport_info[1];
			viewport_info[0].setViewportCount(3);
			viewport_info[0].setPViewports(viewport);
			viewport_info[0].setScissorCount((uint32_t)scissor.size());
			viewport_info[0].setPScissors(scissor.data());

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info[1];
			rasterization_info[0].setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info[0].setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info[0].setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info[0].setLineWidth(1.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);

			// ブレンド
			vk::PipelineColorBlendStateCreateInfo blend_info;

			// vertex input
			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(glm::vec3))
			};
			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				// pos
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(0),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount(vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount(vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(m_make_triangleLL_shader_info.size())
				.setPStages(m_make_triangleLL_shader_info.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info[0])
				.setPRasterizationState(&rasterization_info[0])
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout)
				.setRenderPass(m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline = loader.m_device.createGraphicsPipelines(vk::PipelineCache(), graphics_pipeline_info)[0];
		}

		m_test.setup(loader, this);
	}
	void execute(vk::CommandBuffer cmd, const vk::DescriptorBufferInfo& vertex, const vk::DescriptorBufferInfo& index, const vk::DescriptorBufferInfo indirect, vk::IndexType index_type)
	{
		{
			// transfer
			vk::ImageSubresourceRange brick_image_subresource_range = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
			{
				std::vector<vk::BufferMemoryBarrier> to_transfer = {
					m_triangleLL_count.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
					m_triangleLL_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
				};
				vk::ImageMemoryBarrier to_transfer_image = {
					vk::ImageMemoryBarrier(vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite, vk::ImageLayout::eGeneral, vk::ImageLayout::eTransferDstOptimal)
					.setImage(m_brick_image)
					.setSubresourceRange(brick_image_subresource_range),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, to_transfer, to_transfer_image);
			}

			cmd.fillBuffer(m_triangleLL_count.getBufferInfo().buffer, m_triangleLL_count.getBufferInfo().offset, m_triangleLL_count.getBufferInfo().range, 0);
			cmd.fillBuffer(m_triangleLL_head.getBufferInfo().buffer, m_triangleLL_head.getBufferInfo().offset, m_triangleLL_head.getBufferInfo().range, -1);
			cmd.clearColorImage(m_brick_image, vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(std::array<uint32_t, 4>{0}), brick_image_subresource_range);

			{
				std::vector<vk::BufferMemoryBarrier> to_shader = {
					m_triangleLL_count.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					m_triangleLL_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				vk::ImageMemoryBarrier to_shader_image = {
					vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral)
					.setImage(m_brick_image)
					.setSubresourceRange(brick_image_subresource_range),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, to_shader, to_shader_image);
			}

			{
				TriangleProjection projection;
				glm::vec3 areaMin = m_brick_info.m_area_min.xyz;
				glm::vec3 areaMax = m_brick_info.m_area_max.xyz;
				glm::vec3 size = (areaMax - areaMin);
				glm::vec3 center = size / 2. + areaMin;
				glm::vec3 ortho_min = size / 2.;
				glm::vec3 ortho_max = -size / 2.;
				projection.ortho[0] = glm::ortho(ortho_min.z, ortho_max.z, ortho_min.y, ortho_max.y, 0.01f, size.x);
				projection.ortho[1] = glm::ortho(ortho_min.x, ortho_max.x, ortho_min.z, ortho_max.z, 0.01f, size.y);
				projection.ortho[2] = glm::ortho(ortho_min.x, ortho_max.x, ortho_min.y, ortho_max.y, 0.01f, size.z);
				glm::vec3 eye = center;
				eye.x = areaMin.x;
				projection.view[0] = glm::lookAt(eye, center, glm::vec3(0., 1., 0.));
				eye = center;
				eye.y = areaMax.y;
				projection.view[1] = glm::lookAt(eye, center, glm::vec3(0., 0., 1.));
				eye = center;
				eye.z = areaMin.z;
				projection.view[2] = glm::lookAt(eye, center, glm::vec3(0., 1., 0.));
				m_triangle_projection.subupdate(projection);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eGeometryShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, { m_triangle_projection.getBufferMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite) }, {});
				m_triangle_projection.update(cmd);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, { m_triangle_projection.getBufferMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead) }, {});

			}
		}
		{

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass);
			begin_render_Info.setFramebuffer(m_framebuffer);
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(128, 128)));
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout, 0, m_descriptor_set, {});

			cmd.bindVertexBuffers(0, { vertex.buffer }, { vertex.offset });
			cmd.bindIndexBuffer(index.buffer, index.offset, index_type);
			cmd.drawIndexedIndirect(indirect.buffer, indirect.offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

			cmd.endRenderPass();

			{
				std::vector<vk::BufferMemoryBarrier> to_shader_read = {
					/*m_parent->*/m_triangleLL_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eVertexShader, vk::DependencyFlags(), {}, to_shader_read, {});
			}

		}

	}
	void draw(vk::CommandBuffer cmd)
	{
		m_test.draw(cmd);
	}

};
