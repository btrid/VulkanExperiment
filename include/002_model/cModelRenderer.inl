


#include <btrlib/Define.h>

template<typename T>
void cModelRenderer_t<T>::cModelDrawPipeline::setup(vk::RenderPass render_pass)
{
	const cGPU& gpu = sThreadData::Order().m_gpu;

	auto device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
	// setup shader
	{
		const char* name[] = {
			"Render.vert.spv",
			"Render.frag.spv",
		};
		static_assert(array_length(name) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourcePath() + "shader\\";
		for (size_t i = 0; i < SHADER_NUM; i++) {
			m_shader_list[i] = loadShader(device.getHandle(), path + name[i]);
		}

		m_stage_info[0] = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eVertex)
			.setModule(m_shader_list[0])
			.setPName("main");
		m_stage_info[1] = vk::PipelineShaderStageCreateInfo()
			.setStage(vk::ShaderStageFlagBits::eFragment)
			.setModule(m_shader_list[1])
			.setPName("main");
	}


	// DescriptorSetLayout
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
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setBinding(16),
		};

		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(bindings.size())
			.setPBindings(bindings.data());
		m_descriptor_set_layout = device->createDescriptorSetLayout(descriptor_layout_info);
	}
	// PipelineLayout
	{
		vk::PipelineLayoutCreateInfo pipelinelayout_info;
		pipelinelayout_info.setSetLayoutCount(1);
		pipelinelayout_info.setPSetLayouts(&m_descriptor_set_layout);
//		pipelinelayout_info.
		m_pipeline_layout = device->createPipelineLayout(pipelinelayout_info);
	}

	// DescriptorPool
	{
		std::vector<vk::DescriptorPoolSize> poolSize =
		{
			vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(10),
			vk::DescriptorPoolSize()
			.setType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(3),
		};

		vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo()
			.setMaxSets(1)
			.setPoolSizeCount(poolSize.size())
			.setPPoolSizes(poolSize.data());

		m_descriptor_pool = device->createDescriptorPool(poolInfo);
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
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, size.width, size.height, 0.1f, 1000.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount(scissor.size());
			viewportInfo.setPScissors(scissor.data());

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
//			depth_stencil_info.setDepthBoundsTestEnable()

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

			std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
			{
				vk::VertexInputBindingDescription()
				.setBinding(0)
				.setInputRate(vk::VertexInputRate::eVertex)
				.setStride(sizeof(T::Vertex))
			};

			std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
			{
				// pos
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(0)
				.setFormat(vk::Format::eR32G32B32A32Sfloat)
				.setOffset(0),
				// normal
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(1)
				.setFormat(vk::Format::eR32G32B32A32Sfloat)
				.setOffset(16),
				// texcoord
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(2)
				.setFormat(vk::Format::eR32G32B32A32Sfloat)
				.setOffset(32),
				// boneID
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(3)
				.setFormat(vk::Format::eR32G32B32A32Sint)
				.setOffset(48),
				vk::VertexInputAttributeDescription()
				.setBinding(0)
				.setLocation(4)
				.setFormat(vk::Format::eR32G32B32A32Sfloat)
				.setOffset(64),
				/*		vk::VertexInputAttributeDescription()
				.binding(0)
				.location(5)
				.format(vk::Format::eR32Sint)
				.offset(80),
				*/ 
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount(vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount(vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(m_stage_info.size())
				.setPStages(m_stage_info.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout)
				.setRenderPass(render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = device->createGraphicsPipelines(pipeline_cache, graphics_pipeline_info);
			m_pipeline = pipelines[0];
		}

	}
	// camera
	auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
	CameraGPU camera_gpu;
	camera_gpu.setup(*m_camera);
	{
		uint32_t uniformSize = sizeof(CameraGPU);
		auto graphicsQueue = gpu.getQueueFamilyIndexList(vk::QueueFlagBits::eGraphics);
		vk::BufferCreateInfo buffer_info;
		buffer_info.setQueueFamilyIndexCount(graphicsQueue.size());
		buffer_info.setPQueueFamilyIndices(graphicsQueue.data());
		buffer_info.setUsage(vk::BufferUsageFlagBits::eUniformBuffer);
		buffer_info.setSize(uniformSize);
		m_camera_uniform.mBuffer = device->createBuffer(buffer_info);

		vk::MemoryRequirements memoryRequest = device->getBufferMemoryRequirements(m_camera_uniform.mBuffer);
		vk::MemoryAllocateInfo memAlloc = vk::MemoryAllocateInfo()
			.setAllocationSize(memoryRequest.size)
			.setMemoryTypeIndex(gpu.getMemoryTypeIndex(memoryRequest, vk::MemoryPropertyFlagBits::eHostVisible));
		m_camera_uniform.mMemory = device->allocateMemory(memAlloc);

		char* mem = reinterpret_cast<char*>(device->mapMemory(m_camera_uniform.mMemory, 0, memoryRequest.size, vk::MemoryMapFlags()));
		{
			memcpy_s(mem, uniformSize, &camera_gpu, uniformSize);
		}
		device->unmapMemory(m_camera_uniform.mMemory);
		device->bindBufferMemory(m_camera_uniform.mBuffer, m_camera_uniform.mMemory, 0);

		m_camera_uniform.mBufferInfo
			.setOffset(0)
			.setRange(vk::DeviceSize(uniformSize))
			.setBuffer(m_camera_uniform.mBuffer);

	}
}
template<typename T>
void cModelRenderer_t<T>::cModelComputePipeline::setup()
{
	const auto& gpu = sThreadData::Order().m_gpu;
	auto device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
	auto computeQueues = gpu.getQueueFamilyIndexList(vk::QueueFlagBits::eCompute);

	// setup shader
	{
		const char* name[] = {
			"Clear.comp.spv",
			"AnimationUpdate.comp.spv",
			"MotionUpdate.comp.spv",
			"NodeTransform.comp.spv",
			"CameraCulling.comp.spv",
			"BoneTransform.comp.spv",
		};
		static_assert(array_length(name) == SHADER_NUM, "not equal shader num");

		std::string path = btr::getResourcePath() + "shader\\";
		for (size_t i = 0; i < SHADER_NUM; i++) {
			m_shader_list[i] = loadShader(device.getHandle(), path + name[i]);
			m_stage_info[i].setStage(vk::ShaderStageFlagBits::eCompute);
			m_stage_info[i].setModule(m_shader_list[i]);
			m_stage_info[i].setPName("main");
		}
	}

	// Create compute pipeline
	std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings =
	{
		// Clear
		std::vector<vk::DescriptorSetLayoutBinding>
	{
		vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(8),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(9),
	},
		// AnimationUpdate
		std::vector<vk::DescriptorSetLayoutBinding>
		{
			vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(8),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(9),
		},
			// MotionUpdate
				std::vector<vk::DescriptorSetLayoutBinding>
			{
				vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(8),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(9),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(10),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(11),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(12),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(13),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(14),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(vk::ShaderStageFlagBits::eCompute)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(15),
			},
				// NodeTransform
					std::vector<vk::DescriptorSetLayoutBinding>
				{
					vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(1)
						.setBinding(0),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(8),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(9),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(10),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(11),
				},
					// CameraCulling
						std::vector<vk::DescriptorSetLayoutBinding>
					{
						/*			vk::DescriptorSetLayoutBinding()
						.stageFlags(vk::ShaderStageFlagBits::eCompute)
						.descriptorType(vk::DescriptorType::eUniformBuffer)
						.descriptorCount(1)
						.binding(0),
						*/				vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(8),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(9),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(10),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(11),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(12),
						vk::DescriptorSetLayoutBinding()
						.setStageFlags(vk::ShaderStageFlagBits::eCompute)
						.setDescriptorType(vk::DescriptorType::eStorageBuffer)
						.setDescriptorCount(1)
						.setBinding(13),
					},
						// BoneTransform
						std::vector<vk::DescriptorSetLayoutBinding>
					{
						vk::DescriptorSetLayoutBinding()
							.setStageFlags(vk::ShaderStageFlagBits::eCompute)
							.setDescriptorType(vk::DescriptorType::eUniformBuffer)
							.setDescriptorCount(1)
							.setBinding(0),
							vk::DescriptorSetLayoutBinding()
							.setStageFlags(vk::ShaderStageFlagBits::eCompute)
							.setDescriptorType(vk::DescriptorType::eStorageBuffer)
							.setDescriptorCount(1)
							.setBinding(8),
							vk::DescriptorSetLayoutBinding()
							.setStageFlags(vk::ShaderStageFlagBits::eCompute)
							.setDescriptorType(vk::DescriptorType::eStorageBuffer)
							.setDescriptorCount(1)
							.setBinding(9),
							vk::DescriptorSetLayoutBinding()
							.setStageFlags(vk::ShaderStageFlagBits::eCompute)
							.setDescriptorType(vk::DescriptorType::eStorageBuffer)
							.setDescriptorCount(1)
							.setBinding(10),
							vk::DescriptorSetLayoutBinding()
							.setStageFlags(vk::ShaderStageFlagBits::eCompute)
							.setDescriptorType(vk::DescriptorType::eStorageBuffer)
							.setDescriptorCount(1)
							.setBinding(11),
					},
	};

	m_descriptor_set_layout.reserve(bindings.size());
	m_layout.reserve(bindings.size());
	for (u32 i = 0; i < bindings.size(); i++)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_layout_info = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount(bindings[i].size())
			.setPBindings(bindings[i].data());
		m_descriptor_set_layout.emplace_back(device->createDescriptorSetLayout(descriptor_layout_info));
		auto& set_layout = m_descriptor_set_layout.back();

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
			.setSetLayoutCount(1)
			.setPSetLayouts(&set_layout);
		m_layout.emplace_back(device->createPipelineLayout(pipelineLayoutInfo));
	}

	// DescriptorPool
	{
		std::vector<vk::DescriptorPoolSize> descriptor_pool_size;
		for (auto& binding : bindings)
		{
			for (auto& buffer : binding)
			{
				descriptor_pool_size.emplace_back(buffer.descriptorType, buffer.descriptorCount);
			}
		}
		vk::DescriptorPoolCreateInfo poolInfo = vk::DescriptorPoolCreateInfo()
			.setMaxSets(bindings.size())
			.setPoolSizeCount(descriptor_pool_size.size())
			.setPPoolSizes(descriptor_pool_size.data());

		m_descriptor_pool = device->createDescriptorPool(poolInfo);
	}


	// pipeline cache
	{
		vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
		m_cache = device->createPipelineCache(cacheInfo);
	}
	std::vector<vk::PipelineShaderStageCreateInfo> shader_stage_info = {
		vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_shader_list[SHADER_COMPUTE_CLEAR])
		.setPName("main"),
		vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_shader_list[SHADER_COMPUTE_ANIMATION_UPDATE])
		.setPName("main"),
		vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_shader_list[SHADER_COMPUTE_MOTION_UPDATE])
		.setPName("main"),
		vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_shader_list[SHADER_COMPUTE_NODE_TRANSFORM])
		.setPName("main"),
		vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_shader_list[SHADER_COMPUTE_CULLING])
		.setPName("main"),
		vk::PipelineShaderStageCreateInfo()
		.setStage(vk::ShaderStageFlagBits::eCompute)
		.setModule(m_shader_list[SHADER_COMPUTE_BONE_TRANSFORM])
		.setPName("main"),
	};
	// Create pipeline
	std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
		vk::ComputePipelineCreateInfo()
		.setStage(shader_stage_info[0])
		.setLayout(m_layout[0]),
		vk::ComputePipelineCreateInfo()
		.setStage(shader_stage_info[1])
		.setLayout(m_layout[1]),
		vk::ComputePipelineCreateInfo()
		.setStage(shader_stage_info[2])
		.setLayout(m_layout[2]),
		vk::ComputePipelineCreateInfo()
		.setStage(shader_stage_info[3])
		.setLayout(m_layout[3]),
		vk::ComputePipelineCreateInfo()
		.setStage(shader_stage_info[4])
		.setLayout(m_layout[4]),
		vk::ComputePipelineCreateInfo()
		.setStage(shader_stage_info[5])
		.setLayout(m_layout[5]),
	};

	for (size_t i = 0; i < compute_pipeline_info.size(); i++) {
		auto p = device->createComputePipelines(m_cache, { compute_pipeline_info[i] });
		m_pipeline.insert(m_pipeline.end(), p.begin(), p.end());
	}

}


template<typename T>
cModelRenderer_t<T>::cModelRenderer_t()
{

}

template<typename T>
void cModelRenderer_t<T>::execute(cThreadPool& threadpool)
{
	for (auto it = m_loading_model.begin(); it != m_loading_model.end();)
	{
		if (std::future_status::ready == it->wait_for(std::chrono::seconds(0)))
		{
			std::unique_ptr<T> render = it->get();
			m_gpu.getDevice(0)[0]->getQueue(0, 0).submit() render->setup();
			m_render.emplace_back();
			it = m_loading_model.erase(it);
		}
		else {
			it++;
		}
	}
	for (auto*& render : m_render)
	{
		std::shared_ptr<std::promise<cModel>> task = std::make_shared<std::promise<cModel>>();
		std::shared_future<cModel> modelFuture = task->get_future().share();
		{
			cThreadJob job;
			auto load = [=]() {
				cModel model;
				model.load(m_gpu, btr::getResourcePath() + "002_model\\" + "tiny.x");
				task->set_value(model);
			};
			job.mJob.push_back(load);

			auto finish = [=]() {
				printf("load file %s.\n", modelFuture.get().getFilename().c_str());
			};
			job.mFinish = finish;
		}

		while (modelFuture.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
			printf("wait...\n");
		}
	}
	threadpool.enque()
}

template<typename T>
void cModelRenderer_t<T>::setup(vk::RenderPass render_pass)
{
	m_gpu = sThreadData::Order().m_gpu;
	m_draw_pipeline.setup(render_pass);
//	m_compute_pipeline.setup();
}
