#pragma once
#include "GI2DPhysics.h"
#include "GI2DContext.h"
#include "GI2DRigidbody.h"

GI2DPhysics::GI2DPhysics(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
{
	m_context = context;
	m_gi2d_context = gi2d_context;

	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(13, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(14, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(15, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(16, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(17, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(18, vk::DescriptorType::eStorageBuffer, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_desc_layout[DescLayout_Data] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
	}
	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		vk::DescriptorSetLayoutBinding binding[] = {
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_desc_layout[DescLayout_Make] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
	}

	{
		const char* name[] =
		{
			"Rigid_ToFluid.comp.spv",
			"Rigid_ToFluidWall.comp.spv",

			"RigidMake_Register.comp.spv",
			"RigidMake_MakeJFA.comp.spv",
			"RigidMake_MakeSDF.comp.spv",

			"Voronoi_SetupJFA.comp.spv",
			"Voronoi_MakeJFA.comp.spv",
			"Voronoi_MakeTriangle.comp.spv",
			"Voronoi_SortTriangle.comp.spv",
			"Voronoi_MakePath.comp.spv",

			"RigidMake_MakeRigidBody.vert.spv",
			"RigidMake_MakeRigidBody.geom.spv",
			"RigidMake_MakeRigidBody.frag.spv",
		};
		static_assert(array_length(name) == Shader_Num, "not equal shader num");

		std::string path = btr::getResourceShaderPath();
		for (size_t i = 0; i < array_length(name); i++) {
			m_shader[i] = loadShaderUnique(m_context->m_device.getHandle(), path + name[i]);
		}
	}

	// pipeline layout
	{
		vk::DescriptorSetLayout layouts[] = {
			m_desc_layout[DescLayout_Data].get(),
			m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_ToFluid] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = "PipelineLayout_ToFluid";
		name_info.objectType = vk::ObjectType::ePipelineLayout;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_ToFluid].get());
		m_context->m_device->setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
#endif
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			m_desc_layout[DescLayout_Data].get(),
			m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
			m_desc_layout[DescLayout_Make].get(),
		};
		vk::PushConstantRange ranges[] = {
			vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, 16),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
		pipeline_layout_info.setPPushConstantRanges(ranges);
		m_pipeline_layout[PipelineLayout_MakeRB] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = "PipelineLayout_MakeRigidbody";
		name_info.objectType = vk::ObjectType::ePipelineLayout;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_MakeRB].get());
		m_context->m_device->setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
#endif
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			m_desc_layout[DescLayout_Data].get(),
			m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
			m_desc_layout[DescLayout_Make].get(),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		m_pipeline_layout[PipelineLayout_DestructWall] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = "PipelineLayout_DestructWall";
		name_info.objectType = vk::ObjectType::ePipelineLayout;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_DestructWall].get());
		m_context->m_device->setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
#endif
	}
	{
		vk::DescriptorSetLayout layouts[] = {
			m_desc_layout[DescLayout_Data].get(),
		};
		vk::PushConstantRange ranges[] = {
			vk::PushConstantRange(vk::ShaderStageFlagBits::eCompute, 0, 16),
		};

		vk::PipelineLayoutCreateInfo pipeline_layout_info;
		pipeline_layout_info.setSetLayoutCount(array_length(layouts));
		pipeline_layout_info.setPSetLayouts(layouts);
		pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
		pipeline_layout_info.setPPushConstantRanges(ranges);
		m_pipeline_layout[PipelineLayout_Voronoi] = m_context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = "PipelineLayout_Voronoi";
		name_info.objectType = vk::ObjectType::ePipelineLayout;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_Voronoi].get());
		m_context->m_device->setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
#endif
	}

	// pipeline
	{
		std::array<vk::PipelineShaderStageCreateInfo, 11> shader_info;
		shader_info[0].setModule(m_shader[Shader_ToFluid].get());
		shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[0].setPName("main");
		shader_info[1].setModule(m_shader[Shader_ToFluidWall].get());
		shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[1].setPName("main");
		shader_info[2].setModule(m_shader[Shader_MakeRB_Register].get());
		shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[2].setPName("main");
		shader_info[3].setModule(m_shader[Shader_MakeRB_MakeJFCell].get());
		shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[3].setPName("main");
		shader_info[4].setModule(m_shader[Shader_MakeRB_MakeSDF].get());
		shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[4].setPName("main");
		shader_info[5].setModule(m_shader[Shader_Voronoi_SetupJFA].get());
		shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[5].setPName("main");
		shader_info[6].setModule(m_shader[Shader_Voronoi_MakeJFA].get());
		shader_info[6].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[6].setPName("main");
		shader_info[7].setModule(m_shader[Shader_Voronoi_MakeTriangle].get());
		shader_info[7].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[7].setPName("main");
		shader_info[8].setModule(m_shader[Shader_Voronoi_SortTriangleVertex].get());
		shader_info[8].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[8].setPName("main");
		shader_info[9].setModule(m_shader[Shader_Voronoi_MakePath].get());
		shader_info[9].setStage(vk::ShaderStageFlagBits::eCompute);
		shader_info[9].setPName("main");
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		{
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[0])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[1])
			.setLayout(m_pipeline_layout[PipelineLayout_ToFluid].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[2])
			.setLayout(m_pipeline_layout[PipelineLayout_MakeRB].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[3])
			.setLayout(m_pipeline_layout[PipelineLayout_MakeRB].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[4])
			.setLayout(m_pipeline_layout[PipelineLayout_MakeRB].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[5])
			.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[6])
			.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[7])
			.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[8])
			.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
			vk::ComputePipelineCreateInfo()
			.setStage(shader_info[9])
			.setLayout(m_pipeline_layout[PipelineLayout_Voronoi].get()),
		};
		auto compute_pipeline = m_context->m_device->createComputePipelinesUnique(m_context->m_cache.get(), compute_pipeline_info);
#if USE_DEBUG_REPORT
		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.pObjectName = "Pipeline_ToFluid";
		name_info.objectType = vk::ObjectType::ePipeline;
		name_info.objectHandle = reinterpret_cast<uint64_t &>(compute_pipeline[0].get());
		m_context->m_device->setDebugUtilsObjectNameEXT(name_info, m_context->m_dispach);
#endif

		m_pipeline[Pipeline_ToFluid] = std::move(compute_pipeline[0]);
		m_pipeline[Pipeline_ToFluidWall] = std::move(compute_pipeline[1]);
		m_pipeline[Pipeline_MakeRB_Register] = std::move(compute_pipeline[2]);
		m_pipeline[Pipeline_MakeRB_MakeJFCell] = std::move(compute_pipeline[3]);
		m_pipeline[Pipeline_MakeRB_MakeSDF] = std::move(compute_pipeline[4]);
		m_pipeline[Pipeline_Voronoi_SetupJFA] = std::move(compute_pipeline[5]);
		m_pipeline[Pipeline_Voronoi_MakeJFA] = std::move(compute_pipeline[6]);
		m_pipeline[Pipeline_Voronoi_MakeTriangle] = std::move(compute_pipeline[7]);
		m_pipeline[Pipeline_Voronoi_SortTriangleVertex] = std::move(compute_pipeline[8]);
		m_pipeline[Pipeline_Voronoi_MakePath] = std::move(compute_pipeline[9]);
	}

	// graphics pipeline
	{
		// レンダーパス
		{
			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(0);
			subpass.setPColorAttachments(nullptr);

			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(0);
			renderpass_info.setPAttachments(nullptr);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}
		{
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(0);
			framebuffer_info.setPAttachments(nullptr);
			framebuffer_info.setWidth(1024);
			framebuffer_info.setHeight(1024);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
		}

		{
			vk::PipelineShaderStageCreateInfo shader_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_MakeRB_DestructWall_VS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_MakeRB_DestructWall_GS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eGeometry),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_MakeRB_DestructWall_FS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, 1024.f, 1024.f, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(1024, 1024));
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount(1);
			viewportInfo.setPScissors(&scissor);

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
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
			blend_info.setAttachmentCount((uint32_t)blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayout_DestructWall].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[Pipeline_MakeRB_DestructWall] = std::move(graphics_pipeline[0]);

		}

	}
	{
		b_world = m_context->m_storage_memory.allocateMemory<World>({ 1,{} });
		b_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ RB_NUM_MAX,{} });
		b_rbparticle = m_context->m_storage_memory.allocateMemory<rbParticle>({ RB_PARTICLE_NUM,{} });
		b_rbparticle_map = m_context->m_storage_memory.allocateMemory<uint32_t>({ RB_PARTICLE_BLOCK_NUM_MAX,{} });
		b_collidable_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_collidable = m_context->m_storage_memory.allocateMemory<rbCollidable>({ COLLIDABLE_NUM * gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_fluid_counter = m_context->m_storage_memory.allocateMemory<uint>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_manager = m_context->m_storage_memory.allocateMemory<BufferManage>({ 1,{} });
		b_rb_memory_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM_MAX,{} });
		b_pb_memory_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_PARTICLE_BLOCK_NUM_MAX,{} });
		b_update_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 4,{} });
		b_rb_update_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_NUM_MAX*2,{} });
		b_pb_update_list = m_context->m_storage_memory.allocateMemory<uint>({ RB_PARTICLE_BLOCK_NUM_MAX*2,{} });
		b_voronoi_cell = m_context->m_storage_memory.allocateMemory<VoronoiCell>({ 4096,{} });
		b_voronoi_polygon = m_context->m_storage_memory.allocateMemory<VoronoiPolygon>({ 4096,{} });
		b_voronoi = m_context->m_storage_memory.allocateMemory<int16_t>({ gi2d_context->RenderSize.x*gi2d_context->RenderSize.y,{} });
		b_voronoi_vertex_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 1,{} });
		b_voronoi_vertex = m_context->m_storage_memory.allocateMemory<VoronoiVertex>({ 4096 * 6,{} });
		b_voronoi_path = m_context->m_storage_memory.allocateMemory<int16_t>({ 4096,{} });
		{
			vk::DescriptorSetLayout layouts[] = {
				m_desc_layout[DescLayout_Data].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descset[DescLayout_Data] = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_world.getInfo(),
				b_rigidbody.getInfo(),
				b_rbparticle.getInfo(),
				b_rbparticle_map.getInfo(),
				b_collidable_counter.getInfo(),
				b_collidable.getInfo(),
				b_fluid_counter.getInfo(),
				b_manager.getInfo(),
				b_rb_memory_list.getInfo(),
				b_pb_memory_list.getInfo(),
				b_update_counter.getInfo(),
				b_rb_update_list.getInfo(),
				b_pb_update_list.getInfo(),
				b_voronoi_cell.getInfo(),
				b_voronoi_polygon.getInfo(),
				b_voronoi.getInfo(),
				b_voronoi_vertex_counter.getInfo(),
				b_voronoi_vertex.getInfo(),
				b_voronoi_path.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descset[DescLayout_Data].get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		b_make_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ 1,{} });
		b_make_particle = m_context->m_storage_memory.allocateMemory<rbParticle>({ MAKE_RB_SIZE_MAX,{} });
		b_make_jfa_cell = m_context->m_storage_memory.allocateMemory<i16vec2>({ MAKE_RB_JFA_CELL, {} });
		b_make_param = m_context->m_storage_memory.allocateMemory<RBMakeParam>({ 1,{} });

		{
			vk::DescriptorSetLayout layouts[] = {
				m_desc_layout[DescLayout_Make].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descset[DescLayout_Make] = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_make_rigidbody.getInfo(),
				b_make_particle.getInfo(),
				b_make_jfa_cell.getInfo(),
				b_make_param.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descset[DescLayout_Make].get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}


	}

	auto cmd = m_context->m_cmd_pool->allocCmdTempolary(0);
	{
		m_world.DT = 0.016f;
		m_world.STEP = 100;
		m_world.step = 0;
		m_world.rigidbody_num = 0;
		m_world.rigidbody_max = RB_NUM_MAX;
		m_world.particleblock_max = RB_PARTICLE_BLOCK_NUM_MAX;
		m_world.gpu_index = 0;
		m_world.cpu_index = 1;
		cmd.updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, m_world);
	}

	{
		{
			std::vector<uint> freelist(RB_NUM_MAX);
			for (uint i = 0; i < freelist.size(); i++)
			{
				freelist[i] = i;
			}
			cmd.updateBuffer<uint>(b_rb_memory_list.getInfo().buffer, b_rb_memory_list.getInfo().offset, freelist);
		}
		{
			std::vector<uint> freelist(RB_PARTICLE_BLOCK_NUM_MAX);
			for (uint i = 0; i < freelist.size(); i++)
			{
				freelist[i] = i;
			}
			cmd.updateBuffer<uint>(b_pb_memory_list.getInfo().buffer, b_pb_memory_list.getInfo().offset, freelist);
		}
		cmd.updateBuffer<BufferManage>(b_manager.getInfo().buffer, b_manager.getInfo().offset, BufferManage{ RB_NUM_MAX, RB_PARTICLE_BLOCK_NUM_MAX, 0, 0, 0, 0 });
		std::array<uvec4, 4> ac;
		ac.fill(uvec4(0, 1, 1, 0));
		cmd.updateBuffer<std::array<uvec4, 4>>(b_update_counter.getInfo().buffer, b_update_counter.getInfo().offset, ac);

		cmd.fillBuffer(b_voronoi_vertex_counter.getInfo().buffer, b_voronoi_vertex_counter.getInfo().offset, b_voronoi_vertex_counter.getInfo().range, 0);

		vk::BufferMemoryBarrier to_read[] = {
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			b_manager.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead |vk::AccessFlagBits::eShaderWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
}


void GI2DPhysics::execute(vk::CommandBuffer cmd)
{
	{
		vk::BufferMemoryBarrier to_write[] = {
			b_collidable_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
			0, nullptr, array_length(to_write), to_write, 0, nullptr);
	}
	uint32_t data = 0;
	cmd.fillBuffer(b_collidable_counter.getInfo().buffer, b_collidable_counter.getInfo().offset, b_collidable_counter.getInfo().range, data);
	cmd.fillBuffer(b_fluid_counter.getInfo().buffer, b_fluid_counter.getInfo().offset, b_fluid_counter.getInfo().range, data);

	{
		// 		static uint a;
		// 		World w;
		// 		w.DT = 0.016f;
		// 		w.STEP = 100;
		// 		w.step = 0;
		// 		w.rigidbody_max = RB_NUM_MAX;
		// 		w.particleblock_max = RB_PARTICLE_BLOCK_NUM_MAX;
		m_world.gpu_index = (m_world.gpu_index + 1) % 2;
		m_world.cpu_index = (m_world.cpu_index + 1) % 2;
		cmd.updateBuffer<World>(b_world.getInfo().buffer, b_world.getInfo().offset, m_world);
	}

	{
		vk::BufferMemoryBarrier to_read[] = {
			b_collidable_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			b_fluid_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			b_world.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}
}
void GI2DPhysics::make(vk::CommandBuffer cmd, const uvec4& box)
{
	auto particle_num = box.z * box.w;
	assert(particle_num <= MAKE_RB_SIZE_MAX);

	uint color = glm::packUnorm4x8(vec4(0.f, 0.f, 1.f, 1.f));
	uint edgecolor = glm::packUnorm4x8(vec4(1.f, 0.f, 0.f, 1.f));
	uint linecolor = glm::packUnorm4x8(vec4(0.f, 1.f, 0.f, 1.f));

	rbParticle _def;
	_def.contact_index = -1;
	_def.color = color;
	_def.flag = 0;
	std::vector<vec2> pos(particle_num);
	std::vector<rbParticle> pstate((particle_num+63)/64*64, _def);
	uint32_t block_num = ceil(pstate.size() / (float)RB_PARTICLE_BLOCK_SIZE);

	uint32_t contact_index = 0;
	vec2 size_max = vec2(-999999.f);
	vec2 size_min = vec2(999999.f);
	for (uint32_t y = 0; y < box.w; y++)
	{
		for (uint32_t x = 0; x < box.z; x++)
		{
			uint i = x + y * box.z;
			pos[i] = vec2(box) + rotate(vec2(x, y) + 0.5f, 0.f);
			pstate[i].pos = pos[i];
			pstate[i].pos_old = pos[i];
			pstate[i].contact_index = contact_index++;
			if (y == 0 || y == box.w - 1 || x == 0 || x == box.z - 1)
			{
				pstate[i].color = edgecolor;
			}
			pstate[i].flag |= RBP_FLAG_ACTIVE;

			size_max = glm::max(pos[i], size_max);
			size_min = glm::min(pos[i], size_min);
		}
	}
	ivec2 jfa_max = ivec2(ceil(size_max));
	ivec2 jfa_min = ivec2(trunc(size_min));
	auto area = jfa_max - jfa_min;
	assert(area.x*area.y <= MAKE_RB_JFA_CELL);

	vec2 pos_sum = vec2(0.f);
	vec2 center_of_mass = vec2(0.f);
	for (int32_t i = 0; i < particle_num; i++)
	{
#define CM_WORK_PRECISION (65535.)
		pos_sum += (pos[i] - vec2(jfa_min))* CM_WORK_PRECISION;
	}
	center_of_mass = pos_sum / particle_num / CM_WORK_PRECISION + vec2(jfa_min);


	std::vector<i16vec2> jfa_cell(area.x*area.y, i16vec2(0xffff));
	for (int32_t i = 0; i < particle_num; i++)
	{
		pstate[i].relative_pos = pos[i] - vec2(jfa_min) - center_of_mass;
		pstate[i].local_pos = pos[i] - vec2(jfa_min);

		ivec2 local_pos = ivec2(pstate[i].local_pos);
		jfa_cell[local_pos.x + local_pos.y*area.x] = i16vec2(0xfffe);
	}
	cmd.updateBuffer<i16vec2>(b_make_jfa_cell.getInfo().buffer, b_make_jfa_cell.getInfo().offset, jfa_cell);

	{
		Rigidbody rb;
		rb.R = vec4(1.f, 0.f, 0.f, 1.f);
		rb.cm = center_of_mass;
		rb.flag = 0;
		//	rb.flag |= RB_FLAG_FLUID;
		rb.size_min = jfa_min;
		rb.size_max = jfa_max;
		rb.life = (std::rand() % 10) + 55;
		rb.pnum = particle_num;
		rb.cm_work = pos_sum;
		rb.Apq_work = ivec4(0);
		cmd.updateBuffer<Rigidbody>(b_make_rigidbody.getInfo().buffer, b_make_rigidbody.getInfo().offset, rb);
	}
	{
		RBMakeParam make_param;
		make_param.pb_num = uvec4(block_num, 1, 1, 0);
		make_param.registered_num = uvec4(0, 1, 1, 0);
		make_param.rb_size = area;
		cmd.updateBuffer<RBMakeParam>(b_make_param.getInfo().buffer, b_make_param.getInfo().offset, make_param);
	}
	cmd.updateBuffer<rbParticle>(b_make_particle.getInfo().buffer, b_make_particle.getInfo().offset, pstate);


	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 0, getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 2, getDescriptorSet(GI2DPhysics::DescLayout_Make), {});

		// make jfa
		// 時間のかかるjfaを先に実行したい
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_MakeJFCell].get());
			auto num = app::calcDipatchGroups(uvec3(area, 1), uvec3(8, 8, 1));

			uint area_max = glm::powerOfTwoAbove(glm::max(area.x, area.y));
			for (int distance = area_max >> 1; distance != 0; distance >>= 1)
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_MakeRB].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, 0 });
				cmd.dispatch(num.x, num.y, num.z);
			}
		}

		// register
		{
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_manager.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_rbparticle_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader| vk::PipelineStageFlagBits::eDrawIndirect, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_Register].get());
//			cmd.dispatchIndirect(b_make_param.getInfo().buffer, b_make_param.getInfo().offset + offsetof(RBMakeParam, pb_num));
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}


		// setup
		{
			vk::BufferMemoryBarrier to_read[] =
			{
				b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead|vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader|vk::PipelineStageFlagBits::eDrawIndirect, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_MakeSDF].get());
			cmd.dispatchIndirect(b_make_param.getInfo().buffer, b_make_param.getInfo().offset + offsetof(RBMakeParam, registered_num));
		}
	}

	vk::BufferMemoryBarrier to_read[] =
	{
		b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
//		b_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
		0, nullptr, array_length(to_read), to_read, 0, nullptr);

}


void GI2DPhysics::executeDestructWall(vk::CommandBuffer cmd)
{
	DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

	{
		{
			vk::BufferMemoryBarrier to_write[] = {
				b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);
		}

		{
			Rigidbody rb;
			rb.R = vec4(1.f, 0.f, 0.f, 1.f);
			rb.cm = vec2(0.f);
//			rb.flag = RB_FLAG_FLUID;
			rb.flag = 0;
			rb.life = 100;
			rb.pnum = 0;
			rb.cm_work = ivec2(0);
			rb.Apq_work = ivec4(0);

			cmd.updateBuffer<Rigidbody>(b_make_rigidbody.getInfo().buffer, b_make_rigidbody.getInfo().offset, rb);
		}

		{
			rbParticle _def;
			_def.contact_index = -1;
			_def.flag = 0;
			std::vector<rbParticle> pstate(MAKE_RB_SIZE_MAX, _def);
//			cmd.updateBuffer<rbParticle>(b_make_particle.getInfo().buffer, b_make_particle.getInfo().offset, pstate);
		}

		{
			static uint s_id;
			s_id = (s_id + 1) % 4096;
			RBMakeParam make_param;
			make_param.pb_num = uvec4(0, 1, 1, 0);
			make_param.registered_num = uvec4(0,1,1,0);
			make_param.rb_size = uvec2(0);
			make_param.destruct_voronoi_id = s_id;
			cmd.updateBuffer<RBMakeParam>(b_make_param.getInfo().buffer, b_make_param.getInfo().offset, make_param);
		}
		cmd.fillBuffer(b_make_jfa_cell.getInfo().buffer, b_make_jfa_cell.getInfo().offset, b_make_jfa_cell.getInfo().range, 0xffffffff);

		{
			vk::BufferMemoryBarrier to_read[] = {
				b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}

	}


	_label.insert("GI2DPhysics::DestructWall");
	{

		vk::DescriptorSet descriptorsets[] = {
			m_descset[DescLayout_Data].get(),
			m_gi2d_context->getDescriptorSet(),
			m_descset[DescLayout_Make].get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_DestructWall].get(), 0, array_length(descriptorsets), descriptorsets, 0, nullptr);
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_MakeRB_DestructWall].get());

		vk::RenderPassBeginInfo render_begin_info;
		render_begin_info.setRenderPass(m_render_pass.get());
		render_begin_info.setFramebuffer(m_framebuffer.get());
		render_begin_info.setRenderArea(vk::Rect2D({}, vk::Extent2D(1024, 1024)));
		cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eInline);

		cmd.draw(1, 1, 0, 0);

		cmd.endRenderPass();

	}

	{
		vk::BufferMemoryBarrier to_read[] = {
			b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

	{
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 0, getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 1, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_MakeRB].get(), 2, getDescriptorSet(GI2DPhysics::DescLayout_Make), {});
		{

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_MakeJFCell].get());

			uint area_max = 256;
			auto num = app::calcDipatchGroups(uvec3(area_max, area_max, 1), uvec3(8, 8, 1));

			for (int distance = area_max >> 1; distance != 0; distance >>= 1)
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.pushConstants<uvec2>(m_pipeline_layout[PipelineLayout_MakeRB].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec2{ distance, 0});
				cmd.dispatch(num.x, num.y, num.z);
			}
		}

		{
			// register
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_manager.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_rbparticle_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_Register].get());
			auto num = app::calcDipatchGroups(uvec3(1, 1, 1), uvec3(1, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
			//			cmd.dispatchIndirect(b_make_param.getInfo().buffer, b_make_param.getInfo().offset + offsetof(RBMakeParam, pb_num));

		}


		{
			// setup
			vk::BufferMemoryBarrier to_read[] =
			{
				b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_make_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_make_param.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eDrawIndirect, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeRB_MakeSDF].get());
			cmd.dispatchIndirect(b_make_param.getInfo().buffer, b_make_param.getInfo().offset + offsetof(RBMakeParam, registered_num));
		}
	}

	vk::BufferMemoryBarrier to_read[] =
	{
		b_make_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		b_make_particle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		//		b_jfa_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		b_update_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
		0, nullptr, array_length(to_read), to_read, 0, nullptr);

}

void GI2DPhysics::executeMakeFluidWall(vk::CommandBuffer cmd)
{

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluid].get(), 0, m_descset[DescLayout_Data].get(), {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_ToFluid].get(), 1, m_gi2d_context->getDescriptorSet(), {});

	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFluidWall].get());
		cmd.dispatch(m_gi2d_context->RenderSize.x / 8, m_gi2d_context->RenderSize.y / 8, 1);
	}

}

void GI2DPhysics::executeMakeVoronoi(vk::CommandBuffer cmd)
{
	DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Voronoi].get(), 0, getDescriptorSet(GI2DPhysics::DescLayout_Data), {});

	ivec2 reso = m_gi2d_context->RenderSize;
	int voronoi_cell_num = 0;
	{
		{
			{
				vk::BufferMemoryBarrier to_write[] = { 
					b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite), 
					b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);
			}

			cmd.fillBuffer(b_voronoi.getInfo().buffer, b_voronoi.getInfo().offset, b_voronoi.getInfo().range, -1);
			cmd.fillBuffer(b_voronoi_polygon.getInfo().buffer, b_voronoi_polygon.getInfo().offset, b_voronoi_polygon.getInfo().range, 0);
			cmd.fillBuffer(b_voronoi_vertex_counter.getInfo().buffer, b_voronoi_vertex_counter.getInfo().offset, b_voronoi_vertex_counter.getInfo().range, 0);

			{
				vk::BufferMemoryBarrier to_read[] = { 
					b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
					b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
					b_voronoi_vertex_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}

			// 適当に点を打つ
			uint step = 32;
			uint area = step * 0.6f;
			uint offset = step * 0.2f;
			std::vector<VoronoiCell> points;
			points.reserve(4096);
			for (uint y = 0; y < reso.y; y+= step)
			{
				for (uint x = 0; x < reso.x; x += step)
				{
					uint xx = x + std::rand() % area + offset;
					uint yy = (y + std::rand() % area + offset);
					VoronoiCell cell;
					cell.point = i16vec2(xx, yy);
					points.push_back(cell);

				}

			}
			assert(points.capacity() == 4096);
			voronoi_cell_num = points.size();
			cmd.updateBuffer<VoronoiCell>(b_voronoi_cell.getInfo().buffer, b_voronoi_cell.getInfo().offset, points);

			
			vk::BufferMemoryBarrier to_read[] = { 
				b_voronoi_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead), 
				b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead), 
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

		}

		{
			// voronoiに初期の点を打つ
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_SetupJFA].get());
			cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_Voronoi].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ voronoi_cell_num, 0, reso });
			auto num = app::calcDipatchGroups(uvec3(voronoi_cell_num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		{
			// jump flooding algorithmでボロノイ図生成
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_MakeJFA].get());
			auto num = app::calcDipatchGroups(uvec3(reso, 1), uvec3(8, 8, 1));

			uint reso_max = glm::max(reso.x, reso.y);
			for (int distance = reso_max >> 1; distance != 0; distance >>= 1)
			{
				vk::BufferMemoryBarrier to_read[] = {
					b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_Voronoi].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ distance, 0, reso });
				cmd.dispatch(num.x, num.y, num.z);
			}

		}
	}

	// make triangle
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_voronoi.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_MakeTriangle].get());
		auto num = app::calcDipatchGroups(uvec3(reso, 1), uvec3(8, 8, 1));

		cmd.dispatch(num.x, num.y, num.z);
	}


	// sort triangle vertex
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_voronoi_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_SortTriangleVertex].get());
		auto num = app::calcDipatchGroups(uvec3(4096, 1, 1), uvec3(64, 1, 1));

		cmd.dispatch(num.x, num.y, num.z);
	}

	vk::BufferMemoryBarrier to_read[] = {
		b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
	};
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
		0, nullptr, array_length(to_read), to_read, 0, nullptr);
}
void GI2DPhysics::executeMakeVoronoiPath(vk::CommandBuffer cmd)
{
	// make path
	{
		vk::BufferMemoryBarrier to_read[] = {
			b_voronoi_polygon.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			b_voronoi_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Voronoi].get(), 0, getDescriptorSet(GI2DPhysics::DescLayout_Data), {});
		cmd.pushConstants<uvec4>(m_pipeline_layout[PipelineLayout_Voronoi].get(), vk::ShaderStageFlagBits::eCompute, 0, uvec4{ 123, 345, uvec2(0) });
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Voronoi_MakePath].get());

		cmd.dispatch(1, 1, 1);
	}

}


