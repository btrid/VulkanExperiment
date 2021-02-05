#pragma once

namespace booleanOp
{
struct Context
{
	enum
	{
		DSL_TLAS,
		DSL_Num,
	};
	enum
	{
		Pipeline_Rendering,
		Pipeline_Num,
	};

	vk::UniqueDescriptorPool m_DP;
	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	vk::UniquePipelineLayout m_PL;

	vk::UniqueDescriptorSetLayout m_DSL_Rendering;
	std::array<vk::UniqueDescriptorSet, sGlobal::BUFFER_COUNT_MAX> m_DS_Rendering;

	Context(btr::Context& ctx)
	{
		// descriptor pool
		{
			vk::DescriptorPoolSize pool_size[1];
			pool_size[0].setType(vk::DescriptorType::eAccelerationStructureKHR);
			pool_size[0].setDescriptorCount(200);

			vk::DescriptorPoolCreateInfo pool_info;
			pool_info.setPoolSizeCount(array_length(pool_size));
			pool_info.setPPoolSizes(pool_size);
			pool_info.setMaxSets(200);
			pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet |vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);
			m_DP = ctx.m_device.createDescriptorPoolUnique(pool_info);

		}
			// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eAccelerationStructureKHR, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eAccelerationStructureKHR, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
// 			desc_layout_info.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
// 			vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT setLayoutBindingFlags;
// 			std::vector<vk::DescriptorBindingFlags> descriptorBindingFlags = { vk::DescriptorBindingFlagBits::eUpdateAfterBind };
// 			setLayoutBindingFlags.setBindingFlags(descriptorBindingFlags);
// 
// 			desc_layout_info.pNext = &setLayoutBindingFlags;
			m_DSL[DSL_TLAS] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);

		}
		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DSL_TLAS].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				RenderTarget::s_descriptor_set_layout.get(),
			};

// 			vk::PushConstantRange constants[] = {
// 				vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setOffset(0).setSize(sizeof(ModelDrawParam)),
// 			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
// 			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
// 			pipeline_layout_info.setPPushConstantRanges(constants);
			m_PL = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		// compute pipeline
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Boolean_Rendering.comp.spv", vk::ShaderStageFlagBits::eCompute},
			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[0])
				.setLayout(m_PL.get()),
			};
			m_pipeline[Pipeline_Rendering] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
		}

	}

};

struct Model
{
	enum MyEnum
	{
		ObjectMask_Add = 0x1,
		ObjectMask_Sub = 0x2,
	};

	struct TLAS
	{
		vk::UniqueAccelerationStructureKHR m_TLAS;
		btr::BufferMemory m_TLAS_buffer;
		btr::BufferMemoryEx<vk::AccelerationStructureInstanceKHR> m_instance_buffer;

		std::vector<vk::AccelerationStructureInstanceKHR> m_instance;
	};

	vk::UniqueDescriptorSet m_DS;
	vk::UniqueDescriptorPool m_DP;
	vk::UniqueDescriptorUpdateTemplate m_DUT;

	std::array<TLAS, 2> m_tlas;

	static void Add(vk::CommandBuffer& cmd, btr::Context& ctx, Context& boolean_ctx, Model& base, ::Model& model, const mat3x4& world)
	{
		vk::AccelerationStructureInstanceKHR instance;
		memcpy(&instance.transform, &world, sizeof(world));
		instance.instanceCustomIndex = 0;
		instance.mask = ObjectMask_Add;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = ctx.m_device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(model.m_BLAS.m_AS.get()));

		base.m_tlas[0].m_instance.push_back(instance);
	}
	static void Sub(vk::CommandBuffer& cmd, btr::Context& ctx, Context& boolean_ctx, Model& base, ::Model& model, const mat3x4& world)
	{
		vk::AccelerationStructureInstanceKHR instance;
		memcpy(&instance.transform, &world, sizeof(world));
		instance.instanceCustomIndex = 1;
		instance.mask = ObjectMask_Sub;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = ctx.m_device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(model.m_BLAS.m_AS.get()));

//		base.m_tlas[1].m_instance.push_back(instance);
		base.m_tlas[0].m_instance.push_back(instance);
	}

	static void BuildTLAS(vk::CommandBuffer& cmd, btr::Context& ctx, Context& boolean_ctx, Model& base)
	{
		for (auto& tlas : base.m_tlas)
		{
			auto instance_buffer = ctx.m_storage_memory.allocateMemory<vk::AccelerationStructureInstanceKHR>(tlas.m_instance.size());
			if(!tlas.m_instance.empty())
			{
				for (size_t copyed = 0; copyed < vector_sizeof(tlas.m_instance); )
				{
					size_t size = glm::min(size_t(65536), vector_sizeof(tlas.m_instance) - copyed);
					cmd.updateBuffer(instance_buffer.getInfo().buffer, instance_buffer.getInfo().offset + copyed, size, ((char*)tlas.m_instance.data()) + copyed);
					copyed += size;
				}
				vk::BufferMemoryBarrier barrier[] = { instance_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR), };
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});

			}

			vk::AccelerationStructureGeometryKHR as_geometry;
			as_geometry.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
			as_geometry.geometryType = vk::GeometryTypeKHR::eInstances;
			vk::AccelerationStructureGeometryInstancesDataKHR instance_data;
			instance_data.data.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo().setBuffer(instance_buffer.getInfo().buffer)) + instance_buffer.getInfo().offset;
			as_geometry.geometry.instances = instance_data;

			vk::AccelerationStructureBuildGeometryInfoKHR AS_buildinfo;
			AS_buildinfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
			AS_buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
			AS_buildinfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
			AS_buildinfo.geometryCount = 1;
			AS_buildinfo.pGeometries = &as_geometry;

			uint32_t maxCount = tlas.m_instance.size();
			auto size_info = ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, AS_buildinfo, maxCount);

			auto old_as = std::move(tlas.m_TLAS);
			auto old_buffer = std::move(tlas.m_TLAS_buffer);

			auto AS_buffer = ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
			vk::AccelerationStructureCreateInfoKHR accelerationCI;
			accelerationCI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
			accelerationCI.buffer = AS_buffer.getInfo().buffer;
			accelerationCI.offset = AS_buffer.getInfo().offset;
			accelerationCI.size = AS_buffer.getInfo().range;

			tlas.m_TLAS = ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);
			tlas.m_TLAS_buffer = std::move(AS_buffer);

			AS_buildinfo.dstAccelerationStructure = tlas.m_TLAS.get();

			auto scratch_buffer = ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize);
			AS_buildinfo.scratchData.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo(scratch_buffer.getInfo().buffer)) + scratch_buffer.getInfo().offset;

			vk::AccelerationStructureBuildRangeInfoKHR range[] = { vk::AccelerationStructureBuildRangeInfoKHR(tlas.m_instance.size()) };

			cmd.buildAccelerationStructuresKHR(AS_buildinfo, range);

			sDeleter::Order().enque(std::move(instance_buffer), std::move(scratch_buffer), std::move(old_as), std::move(old_buffer));

			vk::BufferMemoryBarrier to_read[] = { tlas.m_TLAS_buffer.makeMemoryBarrier(vk::AccessFlagBits::eAccelerationStructureWriteKHR, vk::AccessFlagBits::eShaderRead), };
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(to_read), to_read }, {});

		}

		// descriptor set
		{
			//#todo bindafter‚ª“®‚©‚È‚¢‚½‚ßÄƒZƒbƒg
			sDeleter::Order().enque(std::move(base.m_DS));

			vk::DescriptorSetLayout layouts[] =
			{
				boolean_ctx.m_DSL[Context::DSL_TLAS].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(boolean_ctx.m_DP.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			base.m_DS = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::WriteDescriptorSetAccelerationStructureKHR as[2];
			as[0].accelerationStructureCount = 1;
			as[0].pAccelerationStructures = &base.m_tlas[0].m_TLAS.get();
			as[1].accelerationStructureCount = 1;
			as[1].pAccelerationStructures = &base.m_tlas[1].m_TLAS.get();
			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
				.setDescriptorCount(1)
				.setPNext(&as[0])
				.setDstBinding(0)
				.setDstSet(base.m_DS.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
				.setDescriptorCount(1)
				.setPNext(&as[1])
				.setDstBinding(1)
				.setDstSet(base.m_DS.get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}


	}

	static void executeRendering(vk::CommandBuffer& cmd, btr::Context& ctx, Context& boolean_ctx, Model& model, RenderTarget& rt)
	{
		DebugLabel _label(cmd,  __FUNCTION__);

		// render_target‚É‘‚­
		{

			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
			image_barrier[0].setImage(rt.m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, boolean_ctx.m_pipeline[Context::Pipeline_Rendering].get());
		vk::DescriptorSet descs[] =
		{
			model.m_DS.get(),
			sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
			rt.m_descriptor.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, boolean_ctx.m_PL.get(), 0, array_length(descs), descs, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, boolean_ctx.m_pipeline[Context::Pipeline_Rendering].get());

		uvec3 num = app::calcDipatchGroups(uvec3(rt.m_info.extent.width, rt.m_info.extent.height, 1), uvec3(8, 8, 1));
		cmd.dispatch(num.x, num.y, num.z);

	}

};

int main()
{
	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(50.f, 50.f, -500.f);
	camera->getData().m_target = vec3(256.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	auto dc_ctx = std::make_shared<DCContext>(context);
	auto boolean_ctx = std::make_shared<booleanOp::Context>(*context);

	auto model_box = ::Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Box.dae", { 300.f });
	auto model = ::Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Duck.dae", { 5.5f });

	Model boolean_model;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	ModelInstance instance_list[1];
	for (auto& i : instance_list) { i = { vec4(glm::linearRand(vec3(0.f), vec3(500.f)), 0.f), vec4(glm::ballRand(1.f), 100.f) }; }
	ModelInstance instance_list_sub[50];
	for (auto& i : instance_list_sub) { i = { vec4(glm::linearRand(vec3(0.f), vec3(500.f)), 0.f), vec4(glm::ballRand(1.f), 100.f) }; }
	//	for (auto& i : instance_list) { i = { vec4(glm::linearRand(vec3(0.f), vec3(500.f)), 0.f), vec4(vec3(0.f, 0.f, 1.f), 100.f) }; }

	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		for (auto& i : instance_list)
		{
			auto rot = glm::rotation(vec3(0.f, 0.f, 1.f), i.dir.xyz());

			auto world = glm::translate(i.pos.xyz()) * glm::toMat4(rot);
			mat3x4 world3x4 = glm::make_mat3x4(glm::value_ptr(glm::transpose(world)));
			Model::Add(cmd, *context, *boolean_ctx, boolean_model, *model, world3x4);
		}
		for (auto& i : instance_list_sub)
		{
			auto rot = glm::rotation(vec3(0.f, 0.f, 1.f), i.dir.xyz());

			auto world = glm::translate(i.pos.xyz()) * glm::toMat4(rot);
			mat3x4 world3x4 = glm::make_mat3x4(glm::value_ptr(glm::transpose(world)));
			Model::Sub(cmd, *context, *boolean_ctx, boolean_model, *model_box, world3x4);
		}
		booleanOp::Model::BuildTLAS(cmd, *context, *boolean_ctx, boolean_model);

	}


	app.setup();


	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

				{
					booleanOp::Model::executeRendering(cmd, *context, *boolean_ctx, boolean_model, *app.m_window->getFrontBuffer());
				}

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
			context->m_device.waitIdle();

		}
		app.postUpdate();
		printf("%-6.3fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

}