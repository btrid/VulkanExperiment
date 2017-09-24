#pragma once
#include <btrlib/Define.h>
#include <btrlib/Loader.h>
#include <btrlib/VoxelPipeline.h>

struct VoxelizeModelResource
{
	btr::BufferMemory m_vertex;
	btr::BufferMemory m_index;
	btr::BufferMemory m_material;
	btr::BufferMemory m_mesh_info;
	btr::BufferMemory m_indirect;
	uint32_t m_mesh_count;
	uint32_t m_index_count;

	vk::UniqueDescriptorSet m_model_descriptor_set;
};


struct VoxelizeVertex
{
	glm::vec3 pos;
};
struct VoxelizeMesh
{
	std::vector<VoxelizeVertex> vertex;
	std::vector<glm::uvec3> index;
	uint32_t m_material_index;
};
struct VoxelizeMaterial
{
	vec4 albedo;
	vec4 emission;
};
struct VoxelizeMeshInfo
{
	uint32_t material_index;
};

struct VoxelizeModel
{
	std::vector<VoxelizeMesh> m_mesh;
	std::array<VoxelizeMaterial, 16> m_material;
};

struct ModelVoxelize : public Voxelize
{
	enum SHADER
	{
		SHADER_VERTEX_VOXELIZE,
		SHADER_GEOMETRY_VOXELIZE,
		SHADER_FRAGMENT_VOXELIZE,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MODEL_VOXELIZE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_MAKE_VOXEL,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_MAKE_VOXEL,
		PIPELINE_NUM,
	};

	vk::UniqueRenderPass m_make_voxel_pass;
	std::vector<vk::UniqueFramebuffer> m_make_voxel_framebuffer;

	std::vector<std::shared_ptr<VoxelizeModelResource>> m_model_list;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_list;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	vk::UniquePipelineCache m_pipeline_cache;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;

	std::vector<vk::UniqueCommandBuffer> m_make_cmd;
	vk::UniqueDescriptorSetLayout m_model_descriptor_set_layout;
	vk::UniqueDescriptorPool m_model_descriptor_pool;

	void setup(std::shared_ptr<btr::Loader>& loader, VoxelPipeline const * const parent)
	{
		auto& gpu = loader->m_gpu;
		auto& device = gpu.getDevice();

		// renderpass
		{
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(0);
			subpass.setPColorAttachments(nullptr);
			subpass.setPDepthStencilAttachment(nullptr);

			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(0)
				.setPAttachments(nullptr)
				.setSubpassCount(1)
				.setPSubpasses(&subpass);
			m_make_voxel_pass = loader->m_device->createRenderPassUnique(renderpass_info);

			m_make_voxel_framebuffer.resize(loader->m_window->getSwapchain().getBackbufferNum());
			{
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_make_voxel_pass.get());
				framebuffer_info.setAttachmentCount(0);
				framebuffer_info.setPAttachments(nullptr);
				framebuffer_info.setWidth(parent->getVoxelInfo().u_cell_num.x);
				framebuffer_info.setHeight(parent->getVoxelInfo().u_cell_num.y);
				framebuffer_info.setLayers(1);

				for (size_t i = 0; i < m_make_voxel_framebuffer.size(); i++) {
					m_make_voxel_framebuffer[i] = loader->m_device->createFramebufferUnique(framebuffer_info);
				}
			}
		}

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "ModelVoxelize.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "ModelVoxelize.geom.spv",vk::ShaderStageFlagBits::eGeometry },
				{ "ModelVoxelize.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_list[i] = std::move(loadShaderUnique(device.getHandle(), path + shader_info[i].name));
				m_stage_info[i].setStage(shader_info[i].stage);
				m_stage_info[i].setModule(m_shader_list[i].get());
				m_stage_info[i].setPName("main");
			}
		}
		// setup descriptor set layout
		{
			vk::DescriptorSetLayoutCreateInfo layout_info;
			vk::DescriptorSetLayoutBinding bindings[2];
			bindings[0].setDescriptorType(vk::DescriptorType::eStorageBuffer);
			bindings[0].setBinding(0);
			bindings[0].setDescriptorCount(1);
			bindings[0].setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
			bindings[1].setDescriptorType(vk::DescriptorType::eStorageBuffer);
			bindings[1].setBinding(1);
			bindings[1].setDescriptorCount(1);
			bindings[1].setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
			layout_info.setBindingCount(array_length(bindings));
			layout_info.setPBindings(bindings);
			m_model_descriptor_set_layout = device->createDescriptorSetLayoutUnique(layout_info);
		}

		// setup pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				parent->getDescriptorSetLayout(VoxelPipeline::DESCRIPTOR_SET_LAYOUT_VOXELIZE),
				m_model_descriptor_set_layout.get()
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// setup pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Extent3D size;
			size.setWidth(parent->getVoxelInfo().u_cell_num.x);
			size.setHeight(parent->getVoxelInfo().u_cell_num.y);
			size.setDepth(parent->getVoxelInfo().u_cell_num.z);
			vk::Viewport viewports[] = {
				vk::Viewport(0.f, 0.f, (float)size.depth, (float)size.height, 0.f, 1.f),
				vk::Viewport(0.f, 0.f, (float)size.width, (float)size.depth, 0.f, 1.f),
				vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f),
			};
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.depth, size.height)),
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.depth)),
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height)),
			};
			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(array_length(viewports));
			viewport_info.setPViewports(viewports);
			viewport_info.setScissorCount((uint32_t)scissor.size());
			viewport_info.setPScissors(scissor.data());

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setLineWidth(1.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			// ブレンド
			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(VoxelizeVertex))
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
			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::array<vk::PipelineShaderStageCreateInfo, 3> stage_info =
			{
				m_stage_info[SHADER_VERTEX_VOXELIZE],
				m_stage_info[SHADER_GEOMETRY_VOXELIZE],
				m_stage_info[SHADER_FRAGMENT_VOXELIZE],
			};
			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)stage_info.size())
				.setPStages(stage_info.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get())
				.setRenderPass(m_make_voxel_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline[PIPELINE_MAKE_VOXEL] = std::move(device->createGraphicsPipelinesUnique(m_pipeline_cache.get(), graphics_pipeline_info)[0]);
		}


		// model用
		{
			vk::DescriptorPoolSize pool_size[1];
			pool_size[0].setDescriptorCount(20);
			pool_size[0].setType(vk::DescriptorType::eStorageBuffer);
			vk::DescriptorPoolCreateInfo descriptor_pool_info;
			descriptor_pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);
			descriptor_pool_info.setMaxSets(20);
			descriptor_pool_info.setPoolSizeCount(array_length(pool_size));
			descriptor_pool_info.setPPoolSizes(pool_size);
			m_model_descriptor_pool = device->createDescriptorPoolUnique(descriptor_pool_info);

		}

	}

	void draw(std::shared_ptr<btr::Executer>& executer, VoxelPipeline const * const parent, vk::CommandBuffer cmd)
	{
		vk::ImageSubresourceRange range;
		range.setLayerCount(1);
		range.setLevelCount(1);
		range.setAspectMask(vk::ImageAspectFlagBits::eColor);
		range.setBaseArrayLayer(0);
		range.setBaseMipLevel(0);

		{
			// make voxel
			vk::RenderPassBeginInfo begin_render_info;
			begin_render_info.setFramebuffer(m_make_voxel_framebuffer[executer->getGPUFrame()].get());
			begin_render_info.setRenderPass(m_make_voxel_pass.get());
			begin_render_info.setRenderArea(vk::Rect2D({}, { parent->getVoxelInfo().u_cell_num.x, parent->getVoxelInfo().u_cell_num.y }));

			cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_MAKE_VOXEL].get());

			std::vector<vk::DescriptorSet> descriptor_sets = {
				parent->getDescriptorSet(VoxelPipeline::DESCRIPTOR_SET_VOXELIZE),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get(), 0, descriptor_sets, {});

			for (auto& model : m_model_list)
			{
				std::vector<vk::DescriptorSet> descriptor_sets = {
					model->m_model_descriptor_set.get(),
				};
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get(), 1, descriptor_sets, {});
				cmd.bindIndexBuffer(model->m_index.getBufferInfo().buffer, model->m_index.getBufferInfo().offset, vk::IndexType::eUint32);
				cmd.bindVertexBuffers(0, model->m_vertex.getBufferInfo().buffer, model->m_vertex.getBufferInfo().offset);
				cmd.drawIndexedIndirect(model->m_indirect.getBufferInfo().buffer, model->m_indirect.getBufferInfo().offset, model->m_mesh_count, sizeof(vk::DrawIndexedIndirectCommand));
			}
			cmd.endRenderPass();
		}

	}
	void addModel(std::shared_ptr<btr::Executer>& executer, vk::CommandBuffer cmd, const VoxelizeModel& model)
	{
		std::vector<VoxelizeVertex> vertex;
		std::vector<glm::uvec3> index;
		std::vector<VoxelizeMeshInfo> mesh_info;
		std::vector<vk::DrawIndexedIndirectCommand> indirect;
		mesh_info.reserve(model.m_mesh.size());
		indirect.reserve(model.m_mesh.size());

		size_t index_offset = 0;
		size_t vertex_offset = 0;
		for (auto& mesh : model.m_mesh)
		{
			vertex.insert(vertex.end(), mesh.vertex.begin(), mesh.vertex.end());
			auto idx = mesh.index;
			std::for_each(idx.begin(), idx.end(), [=](auto& i) {i += vertex_offset; });
			index.insert(index.end(), idx.begin(), idx.end());

			vk::DrawIndexedIndirectCommand draw_cmd;
			draw_cmd.setFirstIndex(index_offset);
			draw_cmd.setFirstInstance(0);
			draw_cmd.setIndexCount(mesh.index.size() * 3);
			draw_cmd.setInstanceCount(1);
			draw_cmd.setVertexOffset(0);
			indirect.push_back(draw_cmd);

			VoxelizeMeshInfo minfo;
			minfo.material_index = mesh.m_material_index;
			mesh_info.push_back(minfo);

			index_offset += mesh.index.size() * 3;
			vertex_offset += mesh.vertex.size();
		}
		std::shared_ptr<VoxelizeModelResource> resource(std::make_shared<VoxelizeModelResource>());
		resource->m_mesh_count = model.m_mesh.size();
		resource->m_index_count = index_offset;
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = vector_sizeof(vertex);
			resource->m_vertex = executer->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = executer->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, vertex.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_vertex.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_vertex.getBufferInfo().buffer, copy);
		}

		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = vector_sizeof(index);
			resource->m_index = executer->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = executer->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, index.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_index.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_index.getBufferInfo().buffer, copy);
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = vector_sizeof(indirect);
			resource->m_indirect = executer->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = executer->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, indirect.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_indirect.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_indirect.getBufferInfo().buffer, copy);
		}

		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = vector_sizeof(mesh_info);
			resource->m_mesh_info = executer->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = executer->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, mesh_info.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_mesh_info.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_mesh_info.getBufferInfo().buffer, copy);
		}
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = vector_sizeof(model.m_material);
			resource->m_material = executer->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = executer->m_staging_memory.allocateMemory(desc);
			memcpy_s(staging.getMappedPtr(), desc.size, model.m_material.data(), desc.size);

			vk::BufferCopy copy;
			copy.setSize(desc.size);
			copy.setDstOffset(resource->m_material.getBufferInfo().offset);
			copy.setSrcOffset(staging.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, resource->m_material.getBufferInfo().buffer, copy);
		}


		{
			vk::DescriptorSetLayout layouts[] = {
				m_model_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo info;
			info.setDescriptorPool(m_model_descriptor_pool.get());
			info.setDescriptorSetCount(array_length(layouts));
			info.setPSetLayouts(layouts);
			resource->m_model_descriptor_set = std::move(executer->m_device->allocateDescriptorSetsUnique(info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				resource->m_material.getBufferInfo(),
				resource->m_mesh_info.getBufferInfo(),
			};
			vk::WriteDescriptorSet write_desc;
			write_desc.setDescriptorType(vk::DescriptorType::eStorageBuffer);
			write_desc.setDescriptorCount(array_length(storages));
			write_desc.setPBufferInfo(storages);
			write_desc.setDstSet(resource->m_model_descriptor_set.get());
			write_desc.setDstBinding(0);
			executer->m_device->updateDescriptorSets(write_desc, {});
		}
		m_model_list.push_back(resource);
	}

};
