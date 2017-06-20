
#include <002_model/cModelPipeline.h>
#include <002_model/cModelRender.h>
#include <btrlib/Define.h>
#include <btrlib/Shape.h>
#include <btrlib/cModel.h>

void cModelComputePipeline::setup(app::Loader& loader, cModelRenderer& renderer)
{
	const auto& gpu = sThreadLocal::Order().m_gpu;
	auto device = gpu.getDevice(vk::QueueFlagBits::eCompute)[0];

	// setup shader
	{
		struct
		{
			const char* name;
			vk::ShaderStageFlagBits stage;
		}shader_info[] =
		{
			{ "001_Clear.comp.spv",			vk::ShaderStageFlagBits::eCompute },
			{ "002_AnimationUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "003_MotionUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "004_NodeTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "005_CameraCulling.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "006_BoneTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ "Render.vert.spv",vk::ShaderStageFlagBits::eVertex },
			{ "RenderFowardPlus.frag.spv",vk::ShaderStageFlagBits::eFragment },
		};
		static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourcePath() + "shader\\binary\\";
		for (size_t i = 0; i < SHADER_NUM; i++) {
			m_shader_list[i] = loadShader(device.getHandle(), path + shader_info[i].name);
			m_stage_info[i].setStage(shader_info[i].stage);
			m_stage_info[i].setModule(m_shader_list[i]);
			m_stage_info[i].setPName("main");
		}
	}
	{
		m_camera.setup(loader.m_uniform_memory, loader.m_staging_memory);
	}

	// Create compute pipeline
	std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_NUM);
	bindings[DESCRIPTOR_ANIMATION] = {
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(0),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(1),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(2),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(3),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(4),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(5),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(6),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(7),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(8),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setDescriptorCount(1)
		.setBinding(32),
	};
	bindings[DESCRIPTOR_MODEL] = {
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setDescriptorCount(1)
		.setBinding(0),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(1),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(2),
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
		.setDescriptorType(vk::DescriptorType::eStorageBuffer)
		.setDescriptorCount(1)
		.setBinding(3),
	};
	// DescriptorSetLayout
	bindings[DESCRIPTOR_PER_MESH] =
	{
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eFragment)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
		.setBinding(0),
	};

	bindings[DESCRIPTOR_SCENE] =
	{
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eVertex)
		.setDescriptorCount(1)
		.setDescriptorType(vk::DescriptorType::eUniformBuffer)
		.setBinding(0),
	};


	bindings[DESCRIPTOR_LIGHT] =
	{
		vk::DescriptorSetLayoutBinding()
		.setStageFlags(vk::ShaderStageFlagBits::eFragment)
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
	};
	for (u32 i = 0; i < bindings.size(); i++)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(bindings[i].size())
			.setPBindings(bindings[i].data());
		m_descriptor_set_layout[i] = device->createDescriptorSetLayout(descriptor_layout_info);
	}

	{
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout[DESCRIPTOR_MODEL],
				m_descriptor_set_layout[DESCRIPTOR_ANIMATION],
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info = vk::PipelineLayoutCreateInfo()
				.setSetLayoutCount(array_length(layouts))
				.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE] = device->createPipelineLayout(pipeline_layout_info);
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout[DESCRIPTOR_MODEL],
				m_descriptor_set_layout[DESCRIPTOR_PER_MESH],
				m_descriptor_set_layout[DESCRIPTOR_SCENE],
				m_descriptor_set_layout[DESCRIPTOR_LIGHT],
			};
			vk::PushConstantRange constant_range[] = {
				vk::PushConstantRange().setOffset(0).setSize(4).setStageFlags(vk::ShaderStageFlagBits::eFragment),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constant_range));
			pipeline_layout_info.setPPushConstantRanges(constant_range);

			m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = device->createPipelineLayout(pipeline_layout_info);
		}

	}

	// DescriptorPool
	{
		std::vector<vk::DescriptorPoolSize> descriptor_pool_size;
		for (auto& binding : bindings)
		{
			for (auto& buffer : binding)
			{
				descriptor_pool_size.emplace_back(buffer.descriptorType, buffer.descriptorCount*10);
			}
		}
		vk::DescriptorPoolCreateInfo descriptor_pool_info;
// 		descriptor_pool_info.maxSets = bindings.size();
// 		descriptor_pool_info.poolSizeCount = descriptor_pool_size.size();
// 		descriptor_pool_info.pPoolSizes = descriptor_pool_size.data();
		descriptor_pool_info.maxSets = 20;
		descriptor_pool_info.poolSizeCount = descriptor_pool_size.size();
		descriptor_pool_info.pPoolSizes = descriptor_pool_size.data();

		m_descriptor_pool = device->createDescriptorPool(descriptor_pool_info);
	}


	// pipeline cache
	{
		vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
		m_cache = device->createPipelineCache(cacheInfo);
	}
	// Create pipeline
	std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_CLEAR])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE]),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_ANIMATION_UPDATE])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE]),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_MOTION_UPDATE])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE]),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_NODE_TRANSFORM])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE]),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_CULLING])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE]),
		vk::ComputePipelineCreateInfo()
		.setStage(m_stage_info[SHADER_COMPUTE_BONE_TRANSFORM])
		.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_COMPUTE]),
	};

	for (size_t i = 0; i < compute_pipeline_info.size(); i++) {
		auto p = device->createComputePipelines(m_cache, { compute_pipeline_info[i] });
		m_pipeline.insert(m_pipeline.end(), p.begin(), p.end());
	}

	vk::Extent3D size;
	size.setWidth(640);
	size.setHeight(480);
	size.setDepth(1);
	// pipeline
	{
		// キャッシュ
		vk::PipelineCacheCreateInfo pipeline_cache_info = vk::PipelineCacheCreateInfo();
		vk::PipelineCache pipeline_cache = device->createPipelineCache(pipeline_cache_info);
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(1);
			viewport_info.setPViewports(&viewport);
			viewport_info.setScissorCount((uint32_t)scissor.size());
			viewport_info.setPScissors(scissor.data());

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eBack);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
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

			// todo
			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(cModel::Vertex))
			};

			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				// pos
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(0),
				// normal
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(1)
				.setFormat(vk::Format::eR32G32B32Sfloat)
				.setOffset(12),
				// texcoord
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(2)
				.setFormat(vk::Format::eR8G8B8A8Snorm)
				.setOffset(24),
				// boneID
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(3)
				.setFormat(vk::Format::eR8G8B8A8Uint)
				.setOffset(28),
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(4)
				.setFormat(vk::Format::eR8G8B8A8Unorm)
				.setOffset(32),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::array<vk::PipelineShaderStageCreateInfo, 2> stage_info =
			{
				m_stage_info[SHADER_RENDER_VERT],
				m_stage_info[SHADER_RENDER_FRAG],
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
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_RENDER])
				.setRenderPass(loader.m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_graphics_pipeline = device->createGraphicsPipelines(pipeline_cache, graphics_pipeline_info)[0];
		}

	}

	{
		// モデルごとのDescriptorの設定
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = m_descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_descriptor_set_layout[cModelComputePipeline::DESCRIPTOR_SCENE];
		m_descriptor_set_scene = device->allocateDescriptorSets(alloc_info)[0];

		std::vector<vk::DescriptorBufferInfo> uniformBufferInfo = {
			m_camera.getBufferInfo(),
		};
		std::vector<vk::WriteDescriptorSet> write_descriptor_set =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(uniformBufferInfo.size())
			.setPBufferInfo(uniformBufferInfo.data())
			.setDstBinding(0)
			.setDstSet(m_descriptor_set_scene),
		};
		device->updateDescriptorSets(write_descriptor_set, {});
	}
	{
		// ライトのDescriptorの設定
		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = m_descriptor_pool;
		alloc_info.descriptorSetCount = 1;
		alloc_info.pSetLayouts = &m_descriptor_set_layout[cModelComputePipeline::DESCRIPTOR_LIGHT];
		m_descriptor_set_light = device->allocateDescriptorSets(alloc_info)[0];

		std::vector<vk::DescriptorBufferInfo> uniformBufferInfo = {
			renderer.getLight().getLightInfoBufferInfo(),
		};
		std::vector<vk::DescriptorBufferInfo> storageBufferInfo = {
			renderer.getLight().getLightLLHeadBufferInfo(),
			renderer.getLight().getLightLLBufferInfo(),
			renderer.getLight().getLightBufferInfo(),
		};
		std::vector<vk::WriteDescriptorSet> write_descriptor_set =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(uniformBufferInfo.size())
			.setPBufferInfo(uniformBufferInfo.data())
			.setDstBinding(0)
			.setDstSet(m_descriptor_set_light),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(storageBufferInfo.size())
			.setPBufferInfo(storageBufferInfo.data())
			.setDstBinding(1)
			.setDstSet(m_descriptor_set_light),
		};
		device->updateDescriptorSets(write_descriptor_set, {});
	}
}
