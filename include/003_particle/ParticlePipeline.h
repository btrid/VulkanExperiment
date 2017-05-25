#pragma once

#include <array>
#include <memory>
#include <btrlib/BufferMemory.h>
#include <btrlib/cCamera.h>

#include <003_particle/MazeGenerator.h>
#include <003_particle/Geometry.h>

struct ParticleInfo
{
	uint32_t m_max_num;
	uint32_t m_emit_max_num;
};

enum class ParticleFlag : uint32_t
{
	e_
};
struct ParticleData
{
	glm::vec4 m_pos;	//!< xyz:pos w:scale
	glm::vec4 m_vel;	//!< xyz:dir w:not use
	uint32_t m_type;
	uint32_t m_flag;
	float m_life;
	uint32_t _p;
};


vk::DescriptorPool createPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings);

template<typename T, size_t Size>
struct CircleIndex
{
	T m_circle_index;
	CircleIndex() :m_circle_index(){}

	T operator++(int) { T i = m_circle_index; this->operator++(); return i; }
	T operator++() { m_circle_index = (m_circle_index + 1) % Size; return m_circle_index; }
	T operator--(int) { T i = m_circle_index; this->operator--(); return i; }
	T operator--() { m_circle_index = (m_circle_index == 0) ? Size - 1 : m_circle_index-1; return m_circle_index; }
	T operator()()const { return m_circle_index; }

	T get()const { return m_circle_index; }
};

struct Pipeline
{
	vk::Pipeline m_pipeline;
	vk::DescriptorSetLayout m_descriptor_set_layout;
};

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size);
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

		btr::AllocatedMemory m_vertex;
		btr::AllocatedMemory m_index;
		uint32_t m_index_count;
		void setup(Loader& loader, cTriangleLL* const t)
		{
			m_parent = t;
			m_brick_info = m_parent->m_brick_info;
			{
				// メモリ確保
				std::vector<glm::vec3> v;
				std::vector<glm::uvec3> i;
				std::tie(v, i) = Geometry::MakeBox(1.f);
				m_index_count = i.size()*3;
				{
					btr::BufferMemory::Descriptor desc;
					desc.size = vector_sizeof(v);
					m_vertex = loader.m_vertex_memory.allocateMemory(desc);

					desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
					auto staging = loader.m_staging_memory.allocateMemory(desc);
					memcpy(staging.getMappedPtr(), v.data(), desc.size);

					vk::BufferCopy copy;
					copy.setSize(desc.size);
					copy.setSrcOffset(staging.getBufferInfo().offset);
					copy.setDstOffset(m_vertex.getBufferInfo().offset);
					loader.m_cmd.copyBuffer(staging.getBufferInfo().buffer, m_vertex.getBufferInfo().buffer, copy);

				}
				{
					btr::BufferMemory::Descriptor desc;
					desc.size = vector_sizeof(i);
					m_index = loader.m_vertex_memory.allocateMemory(desc);

					desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
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
				std::string path = btr::getResourcePath() + "shader\\binary\\";
				for (size_t i = 0; i < sizeof(shader_desc) / sizeof(shader_desc[0]); i++)
				{
					m_draw_triangleLL_shader_info[i].setModule(loadShader(loader.m_device.getHandle(), path + shader_desc[i].name));
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
				m_descriptor_set_layout = loader.m_device->createDescriptorSetLayout(descriptor_set_layout_info);

				m_descriptor_pool = createPool(loader.m_device.getHandle(), { bindings });

				vk::DescriptorSetAllocateInfo alloc_info;
				alloc_info.descriptorPool = m_descriptor_pool;
				alloc_info.descriptorSetCount = 1;
				alloc_info.pSetLayouts = &m_descriptor_set_layout;
				m_descriptor_set = loader.m_device->allocateDescriptorSets(alloc_info)[0];

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
					m_pipeline_layout = loader.m_device->createPipelineLayout(pipeline_layout_info);
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
					loader.m_device->updateDescriptorSets(write_desc, {});
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
				viewport[0] = vk::Viewport(0.f, 0.f, (float)640, (float)480, 0.f, 1.f);
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
				rasterization_info.setLineWidth(0.5f);
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
				m_pipeline = loader.m_device->createGraphicsPipelines(vk::PipelineCache(), graphics_pipeline_info)[0];
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
			auto pv = camera.mProjection * camera.mView;
			cmd.pushConstants<glm::mat4>(m_pipeline_layout, vk::ShaderStageFlagBits::eVertex, 0, pv);

//			cmd.drawIndexed(m_index_count, /*m_brick_info.m_total_num.w+*/1, 0, 0, 0);
			cmd.drawIndexed(m_index_count, m_brick_info.m_total_num.w+1, 0, 0, 0);
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

	vk::RenderPass m_render_pass;
	vk::Framebuffer m_framebuffer;

	vk::DescriptorPool m_descriptor_pool;
	vk::DescriptorSetLayout m_descriptor_set_layout;
	vk::DescriptorSet m_descriptor_set;
	vk::PipelineLayout m_pipeline_layout;
	vk::Pipeline m_pipeline;

	BrickParam	m_brick_info;
	btr::AllocatedMemory m_triangle_info;
	btr::AllocatedMemory m_triangleLL_head;
	btr::AllocatedMemory m_triangleLL;
	btr::AllocatedMemory m_triangleLL_count;

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

	void setup(Loader& loader)
	{
		{
			// メモリ確保
			{
				// info
				btr::BufferMemory::Descriptor desc;
				desc.size = sizeof(BrickParam);
				m_triangle_info = loader.m_uniform_memory.allocateMemory(desc);

				desc.attribute = btr::BufferMemory::AttributeFlagBits::SHORT_LIVE_BIT;
				auto staging_buffer = loader.m_staging_memory.allocateMemory(desc);
				BrickParam& param = *staging_buffer.getMappedPtr<BrickParam>();

				param.m_cell_num = glm::uvec4(16, 8, 16, 16 * 8 * 16);
				param.m_subcell_num = glm::uvec4(4, 4, 4, 4 * 4 * 4);
// 				param.m_cell_num = glm::uvec4(64, 64, 64, 64 * 64 * 64);
// 				param.m_subcell_num = glm::uvec4(4, 4, 4, 4 * 4 * 4);


				param.m_total_num = param.m_cell_num*param.m_subcell_num;
				param.m_area_min = glm::vec4(0.f, -50.f, 0.f, 0.f);
				param.m_area_max = glm::vec4(4000.f, 50.f, 4000.f, 0.f);
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
				btr::BufferMemory::Descriptor desc;
				desc.size = sizeof(uint32_t)*m_brick_info.m_total_num.w;
				m_triangleLL_head = loader.m_storage_memory.allocateMemory(desc);
			}
			{
				// link list
				btr::BufferMemory::Descriptor desc;
				desc.size = sizeof(TriangleLL)*1000;
				m_triangleLL = loader.m_storage_memory.allocateMemory(desc);
			}
			{
				// triangle counter
				btr::BufferMemory::Descriptor desc;
				desc.size = sizeof(uint32_t);
				m_triangleLL_count = loader.m_storage_memory.allocateMemory(desc);
			}

			{
				vk::ImageCreateInfo image_info;
				image_info.imageType = vk::ImageType::e3D;
				image_info.format = vk::Format::eR8G8B8A8Uint;
				image_info.mipLevels = 1;
				image_info.arrayLayers = 1;
				image_info.samples = vk::SampleCountFlagBits::e1;
				image_info.tiling = vk::ImageTiling::eOptimal;
				image_info.usage = vk::ImageUsageFlagBits::eStorage;
				image_info.sharingMode = vk::SharingMode::eExclusive;
				image_info.initialLayout = vk::ImageLayout::eUndefined;
				image_info.extent = { m_brick_info.m_cell_num.x, m_brick_info.m_cell_num.y, m_brick_info.m_cell_num.z };
				image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
				m_brick_image = loader.m_device->createImage(image_info);

				vk::MemoryRequirements memory_request = loader.m_device->getImageMemoryRequirements(m_brick_image);
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(loader.m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				m_brick_image_memory = loader.m_device->allocateMemory(memory_alloc_info);
				loader.m_device->bindImageMemory(m_brick_image, m_brick_image_memory, 0);

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
				m_brick_image_view = loader.m_device->createImageView(view_info);

				{

					vk::ImageMemoryBarrier to_shader_read_barrier;
					to_shader_read_barrier.dstQueueFamilyIndex = loader.m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
					to_shader_read_barrier.image = m_brick_image;
					to_shader_read_barrier.oldLayout = vk::ImageLayout::eUndefined;
					to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
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
			std::string path = btr::getResourcePath() + "shader\\binary\\";
			for (size_t i = 0; i < sizeof(shader_desc) / sizeof(shader_desc[0]); i++)
			{
				m_make_triangleLL_shader_info[i].setModule(loadShader(loader.m_device.getHandle(), path + shader_desc[i].name));
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
				.setStageFlags(vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
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
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setBinding(16),
			};
			auto descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings.size())
				.setPBindings(bindings.data());
			m_descriptor_set_layout = loader.m_device->createDescriptorSetLayout(descriptor_set_layout_info);

			m_descriptor_pool = createPool(loader.m_device.getHandle(), { bindings });

			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = m_descriptor_pool;
			alloc_info.descriptorSetCount = 1;
			alloc_info.pSetLayouts = &m_descriptor_set_layout;
			m_descriptor_set = loader.m_device->allocateDescriptorSets(alloc_info)[0];

		}

		{
			{
				std::vector<vk::DescriptorSetLayout> layouts = {
					m_descriptor_set_layout,
				};
// 				std::vector<vk::PushConstantRange> push_constants = {
// 					vk::PushConstantRange()
// 					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
// 					.setSize(12),
// 				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(layouts.size());
				pipeline_layout_info.setPSetLayouts(layouts.data());
// 				pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
// 				pipeline_layout_info.setPPushConstantRanges(push_constants.data());
				m_pipeline_layout = loader.m_device->createPipelineLayout(pipeline_layout_info);
			}

			{

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_triangle_info.getBufferInfo(),
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
					.setDstBinding(1)
					.setDstSet(m_descriptor_set),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageImage)
					.setDescriptorCount(images.size())
					.setPImageInfo(images.data())
					.setDstBinding(16)
					.setDstSet(m_descriptor_set),
				};
				loader.m_device->updateDescriptorSets(write_desc, {});
			}

		}
		{
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo();
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);
			m_render_pass = loader.m_device->createRenderPass(renderpass_info);


			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass);
			framebuffer_info.setHeight(256);
			framebuffer_info.setWidth(256);
			m_framebuffer = loader.m_device->createFramebuffer(framebuffer_info);

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
			vk::Viewport viewport_d;
			viewport_d = vk::Viewport(0.f, 0.f, 640.f, 480.f, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor_d = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(viewport_d.width, viewport_d.height)),
			};
			vk::PipelineViewportStateCreateInfo viewportInfo[2];
			viewportInfo[0].setViewportCount(3);
			viewportInfo[0].setPViewports(viewport);
			viewportInfo[0].setScissorCount((uint32_t)scissor.size());
			viewportInfo[0].setPScissors(scissor.data());
			viewportInfo[0].setViewportCount(1);
			viewportInfo[0].setPViewports(&viewport_d);
			viewportInfo[0].setScissorCount((uint32_t)scissor_d.size());
			viewportInfo[0].setPScissors(scissor_d.data());

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info[2];
			rasterization_info[0].setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info[0].setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info[0].setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info[0].setLineWidth(1.f);
			rasterization_info[1].setPolygonMode(vk::PolygonMode::eLine);
			rasterization_info[1].setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info[1].setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info[1].setLineWidth(1.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

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
				.setPViewportState(&viewportInfo[0])
				.setPRasterizationState(&rasterization_info[0])
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout)
				.setRenderPass(m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
// 				vk::GraphicsPipelineCreateInfo()
// 				.setStageCount(m_make_triangleLL_shader_info.size())
// 				.setPStages(m_make_triangleLL_shader_info.data())
// 				.setPVertexInputState(&vertex_input_info)
// 				.setPInputAssemblyState(&assembly_info)
// 				.setPViewportState(&viewportInfo[1])
// 				.setPRasterizationState(&rasterization_info[1])
// 				.setPMultisampleState(&sample_info)
// 				.setLayout(m_pipeline_layout)
// 				.setRenderPass(m_render_pass)
// 				.setPDepthStencilState(&depth_stencil_info)
// 				.setPColorBlendState(&blend_info),
			};
			m_pipeline = loader.m_device->createGraphicsPipelines(vk::PipelineCache(), graphics_pipeline_info)[0];
		}

		m_test.setup(loader, this);
	}
	void execute(vk::CommandBuffer cmd, const vk::DescriptorBufferInfo& vertex, const vk::DescriptorBufferInfo& index, const vk::DescriptorBufferInfo indirect, vk::IndexType index_type)
	{
		// transfer
		{
			cmd.fillBuffer(m_triangleLL_count.getBufferInfo().buffer, m_triangleLL_count.getBufferInfo().offset, m_triangleLL_count.getBufferInfo().range, 0);
		}
		{

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass);
			begin_render_Info.setFramebuffer(m_framebuffer);
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(256, 256)));
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout, 0, m_descriptor_set, {});

			cmd.bindVertexBuffers(0, { vertex.buffer }, { vertex.offset });
			cmd.bindIndexBuffer(index.buffer, index.offset, index_type);
			cmd.drawIndexedIndirect(indirect.buffer, indirect.offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

			cmd.endRenderPass();

		}

	}
	void draw(vk::CommandBuffer cmd)
	{
		m_test.draw(cmd);
	}

};

struct cParticlePipeline
{
	struct Private 
	{
		enum : uint32_t {
			COMPUTE_UPDATE,
			COMPUTE_EMIT,
			COMPUTE_NUM,
		};
		enum : uint32_t {
			GRAPHICS_SHADER_VERTEX_PARTICLE,
			GRAPHICS_SHADER_FRAGMENT_PARTICLE,
			GRAPHICS_SHADER_VERTEX_FLOOR,
			GRAPHICS_SHADER_GEOMETRY_FLOOR,
			GRAPHICS_SHADER_FRAGMENT_FLOOR,
			GRAPHICS_SHADER_NUM,
		};

		enum : uint32_t
		{
			COMPUTE_PIPELINE_LAYOUT_UPDATE,
			COMPUTE_PIPELINE_LAYOUT_EMIT,
			GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW,
			GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW,
			PIPELINE_LAYOUT_NUM,
		};
		enum : uint32_t
		{
			COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE,
			COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT,
			GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA,
			PIPELINE_DESCRIPTOR_SET_LAYOUT_NUM,
		};

		btr::AllocatedMemory m_particle;
		btr::AllocatedMemory m_particle_info;
		btr::UpdateBuffer<std::array<ParticleData, 1024>> m_particle_emit;
//		btr::AllocatedMemory m_particle_emit;
		btr::AllocatedMemory m_particle_counter;
		btr::UpdateBuffer<CameraGPU> m_camera;
		CircleIndex<uint32_t, 2> m_circle_index;

		vk::DescriptorBufferInfo m_particle_counter_info;
		btr::AllocatedMemory m_particle_draw_indiret_info;
		vk::PipelineCache m_cache;
		vk::DescriptorPool m_descriptor_pool;
		std::array<vk::DescriptorSetLayout, PIPELINE_DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
		std::array<vk::DescriptorSet, PIPELINE_DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set;
		std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

		std::vector<vk::Pipeline> m_compute_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, COMPUTE_NUM> m_compute_shader_info;

		std::vector<vk::Pipeline> m_graphics_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, GRAPHICS_SHADER_NUM> m_graphics_shader_info;

		ParticleInfo m_info;

		MazeGenerator m_maze;
		Geometry m_maze_geometry;

		cTriangleLL m_make_triangleLL;

		void setup(Loader& loader)
		{
			{
				m_maze.generate(31, 31);
				auto geometry = m_maze.makeGeometry();
				Geometry::OptimaizeDuplicateVertexDescriptor opti_desc;
				Geometry::OptimaizeDuplicateVertex(geometry, opti_desc);
				auto map = m_maze.makeMapData();
				

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
			}

			m_info.m_max_num = 8192;
			m_info.m_emit_max_num = 1024;

			{
				{
					btr::BufferMemory::Descriptor data_desc;
					data_desc.size = sizeof(ParticleData) * m_info.m_max_num;
					m_particle = loader.m_storage_memory.allocateMemory(data_desc);
					std::vector<ParticleData> p(m_info.m_max_num);
					loader.m_cmd.fillBuffer(m_particle.getBuffer(), m_particle.getOffset(), m_particle.getSize(), 0u);
				}

				{
					btr::UpdateBufferDescriptor emit_desc;
					emit_desc.device_memory = loader.m_storage_memory;
					emit_desc.staging_memory = loader.m_staging_memory;
					emit_desc.frame_max = sGlobal::FRAME_MAX;
					m_particle_emit.setup(emit_desc);
				}

				btr::BufferMemory::Descriptor desc;
				desc.size = sizeof(vk::DrawIndirectCommand);
				m_particle_counter = loader.m_storage_memory.allocateMemory(desc);
				loader.m_cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBuffer(), m_particle_counter.getOffset(), vk::DrawIndirectCommand(1, 1, 0, 0));
				auto count_barrier = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
				loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { count_barrier }, {});

				m_particle_info = loader.m_uniform_memory.allocateMemory(sizeof(ParticleInfo));
				loader.m_cmd.updateBuffer<ParticleInfo>(m_particle_info.getBuffer(), m_particle_info.getOffset(), { m_info });
				auto barrier = m_particle_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
				loader.m_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { barrier }, {});

				btr::UpdateBufferDescriptor update_desc;
				update_desc.device_memory = loader.m_uniform_memory;
				update_desc.staging_memory = loader.m_staging_memory;
				update_desc.frame_max = sGlobal::FRAME_MAX;
				m_camera.setup(update_desc);
			}

			{
				const char* name[] = {
					"ParticleUpdate.comp.spv",
					"ParticleEmit.comp.spv",
				};
				static_assert(array_length(name) == COMPUTE_NUM, "not equal shader num");

				std::string path = btr::getResourcePath() + "shader\\binary\\";
				for (size_t i = 0; i < COMPUTE_NUM; i++)
				{
					m_compute_shader_info[i].setModule(loadShader(loader.m_device.getHandle(), path + name[i]));
					m_compute_shader_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
					m_compute_shader_info[i].setPName("main");
				}
			}

			{
				struct ShaderDesc {
					const char* name;
					vk::ShaderStageFlagBits stage;
				} shader_desc[]=
				{
					{"Render.vert.spv", vk::ShaderStageFlagBits::eVertex },
					{"Render.frag.spv", vk::ShaderStageFlagBits::eFragment },
					{ "FloorRender.vert.spv", vk::ShaderStageFlagBits::eVertex },
					{ "FloorRender.geom.spv", vk::ShaderStageFlagBits::eGeometry },
					{"FloorRender.frag.spv", vk::ShaderStageFlagBits::eFragment },
				};
				static_assert(array_length(shader_desc) == GRAPHICS_SHADER_NUM, "not equal shader num");
				std::string path = btr::getResourcePath() + "shader\\binary\\";
				for (uint32_t i = 0; i < GRAPHICS_SHADER_NUM; i++)
				{
					m_graphics_shader_info[i].setModule(loadShader(loader.m_device.getHandle(), path + shader_desc[i].name));
					m_graphics_shader_info[i].setStage(shader_desc[i].stage);
					m_graphics_shader_info[i].setPName("main");
				}
			}

			{
				// descriptor set layout
				std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(PIPELINE_DESCRIPTOR_SET_LAYOUT_NUM);
				bindings[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE] =
				{
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setBinding(0),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setBinding(1),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setBinding(2),
				};

				vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
					.setBindingCount(bindings[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE].size())
					.setPBindings(bindings[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE].data());
				m_descriptor_set_layout[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE] = loader.m_device->createDescriptorSetLayout(descriptor_set_layout_info);

				bindings[COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT] =
				{
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setBinding(0),
				};
				descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
					.setBindingCount(bindings[COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT].size())
					.setPBindings(bindings[COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT].data());
				m_descriptor_set_layout[COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT] = loader.m_device->createDescriptorSetLayout(descriptor_set_layout_info);

				bindings[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA] =
				{
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eVertex)
					.setDescriptorCount(1)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setBinding(0),
				};
				descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
					.setBindingCount(bindings[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA].size())
					.setPBindings(bindings[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA].data());
				m_descriptor_set_layout[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA] = loader.m_device->createDescriptorSetLayout(descriptor_set_layout_info);

				m_descriptor_pool = createPool(loader.m_device.getHandle(), bindings);
			}
			{
				{
					std::vector<vk::DescriptorSetLayout> layouts = {
						m_descriptor_set_layout[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE],
					};
					std::vector<vk::PushConstantRange> push_constants = {
						vk::PushConstantRange()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setSize(12),
					};
					vk::PipelineLayoutCreateInfo pipeline_layout_info;
					pipeline_layout_info.setSetLayoutCount(layouts.size());
					pipeline_layout_info.setPSetLayouts(layouts.data());
					pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
					pipeline_layout_info.setPPushConstantRanges(push_constants.data());
					m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_UPDATE] = loader.m_device->createPipelineLayout(pipeline_layout_info);
				}
				{
					std::vector<vk::DescriptorSetLayout> layouts = {
						m_descriptor_set_layout[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE],
						m_descriptor_set_layout[COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT],
					};
					std::vector<vk::PushConstantRange> push_constants = {
						vk::PushConstantRange()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setSize(8),
					};
					vk::PipelineLayoutCreateInfo pipeline_layout_info;
					pipeline_layout_info.setSetLayoutCount(layouts.size());
					pipeline_layout_info.setPSetLayouts(layouts.data());
					pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
					pipeline_layout_info.setPPushConstantRanges(push_constants.data());
					m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_EMIT] = loader.m_device->createPipelineLayout(pipeline_layout_info);
				}
				{
					std::vector<vk::DescriptorSetLayout> layouts = {
						m_descriptor_set_layout[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE],
						m_descriptor_set_layout[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA],
					};
					std::vector<vk::PushConstantRange> push_constants = {
						vk::PushConstantRange()
						.setStageFlags(vk::ShaderStageFlagBits::eVertex)
						.setSize(4),
					};
					vk::PipelineLayoutCreateInfo pipeline_layout_info;
					pipeline_layout_info.setSetLayoutCount(layouts.size());
					pipeline_layout_info.setPSetLayouts(layouts.data());
					pipeline_layout_info.setPushConstantRangeCount(push_constants.size());
					pipeline_layout_info.setPPushConstantRanges(push_constants.data());
					m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW] = loader.m_device->createPipelineLayout(pipeline_layout_info);
				}

				{
					std::vector<vk::DescriptorSetLayout> layouts = {
						m_descriptor_set_layout[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA],
					};
					vk::PipelineLayoutCreateInfo pipeline_layout_info;
					pipeline_layout_info.setSetLayoutCount(layouts.size());
					pipeline_layout_info.setPSetLayouts(layouts.data());
					m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW] = loader.m_device->createPipelineLayout(pipeline_layout_info);
				}

				vk::DescriptorSetAllocateInfo alloc_info;
				alloc_info.descriptorPool = m_descriptor_pool;
				alloc_info.descriptorSetCount = m_descriptor_set_layout.size();
				alloc_info.pSetLayouts = m_descriptor_set_layout.data();
				auto descriptor_set = loader.m_device->allocateDescriptorSets(alloc_info);
				std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());

				{

					std::vector<vk::DescriptorBufferInfo> uniforms = {
						m_particle_info.getBufferInfo(),
					};
					std::vector<vk::DescriptorBufferInfo> storages = {
						m_particle.getBufferInfo(),
						m_particle_counter.getBufferInfo(),
					};
					std::vector<vk::WriteDescriptorSet> write_desc =
					{
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(uniforms.size())
						.setPBufferInfo(uniforms.data())
						.setDstBinding(0)
						.setDstSet(m_descriptor_set[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE]),
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(storages.size())
						.setPBufferInfo(storages.data())
						.setDstBinding(1)
						.setDstSet(m_descriptor_set[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE]),
					};
					loader.m_device->updateDescriptorSets(write_desc, {});
				}
				{
					std::vector<vk::DescriptorBufferInfo> storages = {
						m_particle_emit.getBufferInfo(),
					};
					std::vector<vk::WriteDescriptorSet> write_desc =
					{
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(storages.size())
						.setPBufferInfo(storages.data())
						.setDstBinding(0)
						.setDstSet(m_descriptor_set[COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT]),
					};
					loader.m_device->updateDescriptorSets(write_desc, {});
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
						.setDstSet(m_descriptor_set[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA]),
					};
					loader.m_device->updateDescriptorSets(write_desc, {});
				}
			}
			{
				// pipeline cache
				{
					vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
					m_cache = loader.m_device->createPipelineCache(cacheInfo);
				}
				// Create pipeline
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = 
				{
					vk::ComputePipelineCreateInfo()
					.setStage(m_compute_shader_info[0])
					.setLayout(m_pipeline_layout[0]),
					vk::ComputePipelineCreateInfo()
					.setStage(m_compute_shader_info[1])
					.setLayout(m_pipeline_layout[1]),
				};
				m_compute_pipeline = loader.m_device->createComputePipelines(m_cache, compute_pipeline_info);

				vk::Extent3D size;
				size.setWidth(640);
				size.setHeight(480);
				size.setDepth(1);
				// pipeline
				{
					// assembly
					vk::PipelineInputAssemblyStateCreateInfo assembly_info[] = 
					{
						vk::PipelineInputAssemblyStateCreateInfo()
						.setPrimitiveRestartEnable(VK_FALSE)
						.setTopology(vk::PrimitiveTopology::ePointList),
						vk::PipelineInputAssemblyStateCreateInfo()
						.setPrimitiveRestartEnable(VK_FALSE)
						.setTopology(vk::PrimitiveTopology::eTriangleList),
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

					// ラスタライズ
					vk::PipelineRasterizationStateCreateInfo rasterization_info;
					rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
//					rasterization_info.setCullMode(vk::CullModeFlagBits::eBack);
					rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
					rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
					rasterization_info.setLineWidth(1.f);
					// サンプリング
					vk::PipelineMultisampleStateCreateInfo sample_info;
					sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

					// デプスステンシル
					vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
					depth_stencil_info.setDepthTestEnable(VK_TRUE);
					depth_stencil_info.setDepthWriteEnable(VK_TRUE);
					depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
					depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
					depth_stencil_info.setStencilTestEnable(VK_FALSE);

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
						// pos
						vk::VertexInputAttributeDescription()
						.setBinding(0)
						.setLocation(0)
						.setFormat(vk::Format::eR32G32B32Sfloat)
						.setOffset(0),
					};

					vertex_input_info[1].setVertexBindingDescriptionCount(vertex_input_binding.size());
					vertex_input_info[1].setPVertexBindingDescriptions(vertex_input_binding.data());
					vertex_input_info[1].setVertexAttributeDescriptionCount(vertex_input_attribute.size());
					vertex_input_info[1].setPVertexAttributeDescriptions(vertex_input_attribute.data());

					std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
					{
						vk::GraphicsPipelineCreateInfo()
						.setStageCount(2)
						.setPStages(m_graphics_shader_info.data())
						.setPVertexInputState(&vertex_input_info[0])
						.setPInputAssemblyState(&assembly_info[0])
						.setPViewportState(&viewportInfo)
						.setPRasterizationState(&rasterization_info)
						.setPMultisampleState(&sample_info)
						.setLayout(m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW])
						.setRenderPass(loader.m_render_pass)
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&blend_info),
						vk::GraphicsPipelineCreateInfo()
						.setStageCount(3)
						.setPStages(m_graphics_shader_info.data()+2)
						.setPVertexInputState(&vertex_input_info[1])
						.setPInputAssemblyState(&assembly_info[1])
						.setPViewportState(&viewportInfo)
						.setPRasterizationState(&rasterization_info)
						.setPMultisampleState(&sample_info)
						.setLayout(m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW])
						.setRenderPass(loader.m_render_pass)
						.setPDepthStencilState(&depth_stencil_info)
						.setPColorBlendState(&blend_info),
					};
					m_graphics_pipeline = loader.m_device->createGraphicsPipelines(m_cache, graphics_pipeline_info);

				}
			}

			m_make_triangleLL.setup(loader);
		}

		void execute(vk::CommandBuffer cmd)
		{
			m_make_triangleLL.execute(cmd, m_maze_geometry.m_resource->m_vertex.getBufferInfo(), m_maze_geometry.m_resource->m_index.getBufferInfo(), m_maze_geometry.m_resource->m_indirect.getBufferInfo(), m_maze_geometry.m_resource->m_index_type);
			{
				// transfer
				std::vector<vk::BufferMemoryBarrier> to_transfer = { 
					m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
					m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBuffer(), m_particle_counter.getOffset(), vk::DrawIndirectCommand(0, 1, 0, 0));

				auto* camera = cCamera::sCamera::Order().getCameraList()[0];
				CameraGPU camera_GPU;
				camera_GPU.setup(*camera);
				m_camera.subupdate(camera_GPU);
				m_camera.update(cmd);

				std::vector<vk::BufferMemoryBarrier> to_update_barrier = {
					m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_update_barrier }, {});

				std::vector<vk::BufferMemoryBarrier> to_draw_barrier = {
					m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_draw_barrier }, {});
			}

			m_circle_index++;
			uint src_offset = m_circle_index.get() == 1 ? (m_particle.getSize()/sizeof(ParticleData) / 2) : 0;
			uint dst_offset = m_circle_index.get() == 0 ? (m_particle.getSize() / sizeof(ParticleData) / 2) : 0;
			{
				// update
				struct UpdateConstantBlock
				{
					float m_deltatime;
					uint m_src_offset;
					uint m_dst_offset;
				};
				UpdateConstantBlock block;
				block.m_deltatime = sGlobal::Order().getDeltaTime();
				block.m_src_offset = src_offset;
				block.m_dst_offset = dst_offset;
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[COMPUTE_UPDATE], vk::ShaderStageFlagBits::eCompute, 0, block);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline[COMPUTE_UPDATE]);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_UPDATE], 0, m_descriptor_set[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE], {});
				auto groups = calcDipatchGroups(glm::uvec3(8192 / 2, 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);

			}


			{
				static int count;
				count++;
				count %= 3000;
				if (count == 1 )
				{
					auto particle_barrier = m_particle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite);
					std::vector<vk::BufferMemoryBarrier> to_emit_barrier =
					{
						particle_barrier,
						m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_emit_barrier, {});

					// emit
					std::array<ParticleData, 200> data;
					for (auto& p : data)
					{
						p.m_pos = glm::vec4(glm::ballRand(500.f), 5.f);
						p.m_vel = glm::vec4(glm::ballRand(10.f), 0.f);
						p.m_life = std::rand() % 50 + 240;
					}
					m_particle_emit.subupdate(data.data(), vector_sizeof(data), 0);

					auto to_transfer = m_particle_emit.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});
					m_particle_emit.update(cmd);
					auto to_read = m_particle_emit.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

					struct EmitConstantBlock
					{
						uint m_emit_num;
						uint m_offset;
					};
					EmitConstantBlock block;
					block.m_emit_num = data.size();
					block.m_offset = dst_offset;
					cmd.pushConstants<EmitConstantBlock>(m_pipeline_layout[COMPUTE_EMIT], vk::ShaderStageFlagBits::eCompute, 0, block);
					cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline[COMPUTE_EMIT]);
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_EMIT], 0, m_descriptor_set[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE], {});
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_EMIT], 1, m_descriptor_set[COMPUTE_DESCRIPTOR_SET_LAYOUT_EMIT], {});
					auto groups = calcDipatchGroups(glm::uvec3(block.m_emit_num, 1, 1), glm::uvec3(1024, 1, 1));
					cmd.dispatch(groups.x, groups.y, groups.z);

				}

				struct DrawConstantBlock
				{
					uint m_src_offset;
				};
				DrawConstantBlock block;
				block.m_src_offset = dst_offset;
				cmd.pushConstants<DrawConstantBlock>(m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW], vk::ShaderStageFlagBits::eVertex, 0, block);
				auto particle_barrier = m_particle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, particle_barrier, {});

				auto to_draw = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});
			}

		}

		void draw(vk::CommandBuffer cmd)
		{
			m_make_triangleLL.draw(cmd);
// 			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline[1]);
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW], 0, m_descriptor_set[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA], {});
// 			cmd.bindVertexBuffers(0, { m_maze_geometry.m_resource->m_vertex.getBuffer() }, { m_maze_geometry.m_resource->m_vertex.getOffset() });
// 			cmd.bindIndexBuffer(m_maze_geometry.m_resource->m_index.getBuffer(), m_maze_geometry.m_resource->m_index.getOffset(), m_maze_geometry.m_resource->m_index_type);
// 			cmd.drawIndexedIndirect(m_maze_geometry.m_resource->m_indirect.getBuffer(), m_maze_geometry.m_resource->m_indirect.getOffset(), 1, sizeof(vk::DrawIndexedIndirectCommand));
// 
// 			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline[0]);
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW], 0, m_descriptor_set[COMPUTE_DESCRIPTOR_SET_LAYOUT_UPDATE], {});
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW], 1, m_descriptor_set[GRAPHICS_DESCRIPTOR_SET_LAYOUT_DRAW_CAMERA], {});
// 			cmd.drawIndirect(m_particle_counter.getBuffer(), m_particle_counter.getOffset(), 1, sizeof(vk::DrawIndirectCommand));
		}

	};
	std::unique_ptr<Private> m_private;

	void setup(Loader& loader)
	{
		auto p = std::make_unique<Private>();
		p->setup(loader);

		m_private = std::move(p);
	}

	void execute(vk::CommandBuffer cmd) 
	{
		m_private->execute(cmd);
	}

	void draw(vk::CommandBuffer cmd)
	{
		m_private->draw(cmd);
	}
};

