#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/cMotion.h>
#include <applib/cLightPipeline.h>
#include <applib/cModelPipeline.h>
#include <applib/cModelInstancingRender.h>

struct ModelInstancingRenderPipelineComponent : public PipelineComponent
{
	ModelInstancingRenderPipelineComponent(const std::shared_ptr<btr::Context>& context)
	{
		auto& device = context->m_device;
		m_render_pass = std::make_shared<RenderPassModule>(context);
		m_model_descriptor = std::make_shared<ModelDescriptorModule>(context);

		m_light_pipeline = std::make_shared<cFowardPlusPipeline>();
		m_light_pipeline->setup(context);

		// setup shader
		{
			std::string path = btr::getResourceLibPath() + "shader\\binary\\";
			std::vector<ShaderDescriptor> shader_desc =
			{
				{ path+"Render.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ path+"RenderFowardPlus.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			assert(shader_desc.size() == SHADER_NUM);
			m_shader = std::make_shared<ShaderModule>(context, shader_desc);
		}


		{
			vk::DescriptorSetLayout layouts[] = {
				m_model_descriptor->getLayout(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				m_light_pipeline->getDescriptorSetLayout(cFowardPlusPipeline::DESCRIPTOR_SET_LAYOUT_LIGHT),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			m_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			vk::Extent3D size;
			size.setWidth(context->m_window->getClientSize().x);
			size.setHeight(context->m_window->getClientSize().y);
			size.setDepth(1);
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

			// vertexinput
			auto vertex_input_binding = cModel::GetVertexInputBinding();
			auto vertex_input_attribute = cModel::GetVertexInputAttribute();
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			const auto& shader_stage = m_shader->getShaderStageInfo();
			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)shader_stage.size())
				.setPStages(shader_stage.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout.get())
				.setRenderPass(m_render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_graphics_pipeline = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}
	std::shared_ptr<ModelInstancingRender> createRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum, std::shared_ptr<ModelInstancingModel>& model)
	{
		std::shared_ptr<ModelInstancingRender> render = std::make_shared<ModelInstancingRender>();
		// setup draw
		{
			auto& device = context->m_device;
			{
				// meshごとの更新
				render->m_model_descriptor_set = m_model_descriptor->allocateDescriptorSet(context);
				m_model_descriptor->updateMaterial(context, render->m_model_descriptor_set.get(), model->m_material);
				m_model_descriptor->updateInstancing(context, render->m_model_descriptor_set.get(), model->m_instancing);
				m_model_descriptor->updateAnimation(context, render->m_model_descriptor_set.get(), model->m_instancing);
			}
		}

		// recode draw command
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = context->m_window->getSwapchain().getBackbufferNum();
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			render->m_draw_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);

			for (size_t i = 0; i < render->m_draw_cmd.size(); i++)
			{
				auto& cmd = render->m_draw_cmd[i].get();

				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
				vk::CommandBufferInheritanceInfo inheritance_info;
				inheritance_info.setFramebuffer(m_render_pass->getFramebuffer(i));
				inheritance_info.setRenderPass(m_render_pass->getRenderPass());
				begin_info.pInheritanceInfo = &inheritance_info;

				cmd.begin(begin_info);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline.get());
				std::vector<vk::DescriptorSet> sets = {
					render->m_model_descriptor_set.get(),
					sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
					m_light_pipeline->getDescriptorSet(cFowardPlusPipeline::DESCRIPTOR_SET_LIGHT),
				};
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, sets, {});

				cmd.bindVertexBuffers(0, { resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
				cmd.bindIndexBuffer(resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, resource->m_mesh_resource.mIndexType);
				cmd.drawIndexedIndirect(model->m_instancing->getDrawIndirect().buffer, model->m_instancing->getDrawIndirect().offset, resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));

				cmd.end();
			}
		}
		return render;

	}

	enum 
	{
		SHADER_RENDER_VERT,
		SHADER_RENDER_FRAG,
		SHADER_NUM,
	};

	std::shared_ptr<RenderPassModule> m_render_pass;
	std::shared_ptr<ShaderModule> m_shader;

	vk::UniquePipeline m_graphics_pipeline;
	vk::UniquePipelineLayout m_pipeline_layout;

	std::shared_ptr<ModelDescriptorModule> m_model_descriptor;
	std::shared_ptr<cFowardPlusPipeline> m_light_pipeline;

};

struct InstancingAnimationDescriptorModule : public DescriptorModule
{
	InstancingAnimationDescriptorModule(const std::shared_ptr<btr::Context>& context)
	{
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
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
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(1)
			.setBinding(32),
		};
		m_descriptor_set_layout = createDescriptorSetLayout(context, binding);
		m_descriptor_pool = createDescriptorPool(context, binding, 30);
	}

	void updateAnimation(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set, const std::shared_ptr<InstancingAnimationModule>& animation)
	{
		std::vector<vk::DescriptorBufferInfo> storages = {
			animation->getModelInfo(),
			animation->getInstancingInfo(),
			animation->getAnimationInfoBuffer(),
			animation->getPlayingAnimationBuffer(),
			animation->getNodeInfoBuffer(),
			animation->getBoneInfoBuffer(),
			animation->getNodeBuffer(),
			animation->getWorldBuffer(),
			animation->getBoneMap(),
			animation->getDrawIndirect(),
			animation->getBoneBuffer(),
		};

		std::vector<vk::DescriptorImageInfo> images =
		{
			vk::DescriptorImageInfo()
			.setImageView(animation->getMotionTexture()[0].getImageView())
			.setSampler(animation->getMotionTexture()[0].getSampler())
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::vector<vk::WriteDescriptorSet> write =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount((uint32_t)storages.size())
			.setPBufferInfo(storages.data())
			.setDstBinding(0)
			.setDstSet(descriptor_set),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(images.size())
			.setPImageInfo(images.data())
			.setDstBinding(32)
			.setDstSet(descriptor_set),
		};
		context->m_device->updateDescriptorSets(write, {});
	}
};

struct ModelInstancingAnimationPipelineComponent : public PipelineComponent
{
	ModelInstancingAnimationPipelineComponent(const std::shared_ptr<btr::Context>& context)
	{
		std::string path = btr::getResourceLibPath() + "shader\\binary\\";
		std::vector<ShaderDescriptor> shader_desc =
		{
			{ path+"001_Clear.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ path+"002_AnimationUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path+"003_MotionUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path+"004_NodeTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path+"005_CameraCulling.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path+"006_BoneTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
		};
		assert(shader_desc.size() == SHADER_NUM);
		m_compute_shader = std::make_shared<ShaderModule>(context, shader_desc);
		m_animation_descriptor = std::make_shared<InstancingAnimationDescriptorModule>(context);

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_animation_descriptor->getLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info = vk::PipelineLayoutCreateInfo()
				.setSetLayoutCount(array_length(layouts))
				.setPSetLayouts(layouts);
			m_pipeline_layout = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// Create pipeline
		std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader->getShaderStageInfo()[SHADER_COMPUTE_CLEAR])
			.setLayout(m_pipeline_layout.get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader->getShaderStageInfo()[SHADER_COMPUTE_ANIMATION_UPDATE])
			.setLayout(m_pipeline_layout.get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader->getShaderStageInfo()[SHADER_COMPUTE_MOTION_UPDATE])
			.setLayout(m_pipeline_layout.get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader->getShaderStageInfo()[SHADER_COMPUTE_NODE_TRANSFORM])
			.setLayout(m_pipeline_layout.get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader->getShaderStageInfo()[SHADER_COMPUTE_CULLING])
			.setLayout(m_pipeline_layout.get()),
			vk::ComputePipelineCreateInfo()
			.setStage(m_compute_shader->getShaderStageInfo()[SHADER_COMPUTE_BONE_TRANSFORM])
			.setLayout(m_pipeline_layout.get()),
		};

		auto p = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
		for (size_t i = 0; i < p.size(); i++) {
			m_pipeline[i] = std::move(p[i]);
		}

	}
	std::shared_ptr<ModelInstancingModule> createRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource, uint32_t instanceNum)
	{
		assert(!resource->mBone.empty());

		auto instancing_module = std::make_shared<ModelInstancingModule>();

		auto cmd_ = context->m_cmd_pool->allocCmdTempolary(0);
		auto cmd = cmd_.get();
		// node info
		{
			auto nodeInfo = ModelInstancingModule::NodeInfo::createNodeInfo(resource->mNodeRoot);
			btr::AllocatedMemory::Descriptor desc;
			desc.size = vector_sizeof(nodeInfo);

			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::NODE_INFO);
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), desc.size, nodeInfo.data(), desc.size);

			vk::BufferCopy copy_info;
			copy_info.setSize(desc.size);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

			auto to_render = buffer.makeMemoryBarrierEx();
			to_render.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_render, {});

		}

		{
			// BoneInfo
			auto bone_info = ModelInstancingModule::BoneInfo::createBoneInfo(resource->mBone);
			btr::AllocatedMemory::Descriptor desc;
			desc.size = vector_sizeof(bone_info);

			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::BONE_INFO);
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			btr::BufferMemory staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), desc.size, bone_info.data(), desc.size);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}

		// BoneTransform
		{
			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::BONE_TRANSFORM);
			buffer = context->m_storage_memory.allocateMemory(resource->mBone.size() * instanceNum * sizeof(mat4));
		}
		// NodeTransform
		{
			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::NODE_TRANSFORM);
			buffer = context->m_storage_memory.allocateMemory(resource->mNodeRoot.mNodeList.size() * instanceNum * sizeof(mat4));
		}

		// ModelInfo
		{
			btr::AllocatedMemory::Descriptor staging_desc;
			staging_desc.size = sizeof(cModel::ModelInfo);
			staging_desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_model_info = context->m_staging_memory.allocateMemory(staging_desc);

			auto& mi = *static_cast<cModel::ModelInfo*>(staging_model_info.getMappedPtr());
			mi = resource->m_model_info;

			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::MODEL_INFO);
			buffer = context->m_storage_memory.allocateMemory(sizeof(cModel::ModelInfo));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_model_info.getBufferInfo().range);
			copy_info.setSrcOffset(staging_model_info.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging_model_info.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

		}

		//ModelInstancingInfo
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
			desc.element_num = 1;
			instancing_module->m_instancing_info_buffer.setup(desc);
		}
		// world
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_storage_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.frame_max = context->m_window->getSwapchain().getBackbufferNum();
			desc.element_num = instanceNum;
			instancing_module->m_world_buffer.setup(desc);
		}

		//BoneMap
		{
			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::BONE_MAP);
			buffer = context->m_storage_memory.allocateMemory(instanceNum * sizeof(s32));
		}
		// draw indirect
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(cModel::Mesh)*resource->m_mesh.size();

			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::DRAW_INDIRECT);
			buffer = context->m_vertex_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);

			memcpy_s(staging.getMappedPtr(), desc.size, resource->m_mesh.data(), desc.size);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

		}

		{
			auto& buffer = instancing_module->m_compute_indirect_buffer;
			buffer = context->m_vertex_memory.allocateMemory(sizeof(glm::ivec3) * 6);

			auto staging = context->m_staging_memory.allocateMemory(sizeof(glm::ivec3) * 6);
			auto* group_ptr = static_cast<glm::ivec3*>(staging.getMappedPtr());
			int32_t local_size_x = 1024;
			// shaderのlocal_size_xと合わせる
			std::vector<glm::ivec3> group =
			{
				glm::ivec3(1, 1, 1),
				glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum*resource->m_model_info.mNodeNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum*resource->m_model_info.mNodeNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum + local_size_x - 1) / local_size_x, 1, 1),
				glm::ivec3((instanceNum*resource->m_model_info.mBoneNum + local_size_x - 1) / local_size_x, 1, 1),
			};
			memcpy_s(group_ptr, sizeof(glm::ivec3) * 6, group.data(), sizeof(glm::ivec3) * 6);

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(instancing_module->m_compute_indirect_buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, instancing_module->m_compute_indirect_buffer.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier dispatch_indirect_barrier;
			dispatch_indirect_barrier.setBuffer(instancing_module->m_compute_indirect_buffer.getBufferInfo().buffer);
			dispatch_indirect_barrier.setOffset(instancing_module->m_compute_indirect_buffer.getBufferInfo().offset);
			dispatch_indirect_barrier.setSize(instancing_module->m_compute_indirect_buffer.getBufferInfo().range);
			dispatch_indirect_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			dispatch_indirect_barrier.setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eDrawIndirect,
				vk::DependencyFlags(),
				{}, { dispatch_indirect_barrier }, {});
		}

		{
			auto& anim = resource->getAnimation();
			instancing_module->m_motion_texture = MotionTexture::createMotion(context, cmd, resource->getAnimation());

			btr::AllocatedMemory::Descriptor desc;
			desc.size = sizeof(ModelInstancingModule::AnimationInfo) * anim.m_motion.size();

			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::ANIMATION_INFO);
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(desc);
			auto* staging_ptr = staging.getMappedPtr<ModelInstancingModule::AnimationInfo>();
			for (size_t i = 0; i < anim.m_motion.size(); i++)
			{
				auto& animation = staging_ptr[i];
				animation.duration_ = (float)anim.m_motion[i]->m_duration;
				animation.ticksPerSecond_ = (float)anim.m_motion[i]->m_ticks_per_second;
			}

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);

			vk::BufferMemoryBarrier barrier;
			barrier.setBuffer(buffer.getBufferInfo().buffer);
			barrier.setOffset(buffer.getBufferInfo().offset);
			barrier.setSize(buffer.getBufferInfo().range);
			barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eComputeShader,
				vk::DependencyFlags(),
				{}, { barrier }, {});
		}
		// PlayingAnimation
		{
			btr::AllocatedMemory::Descriptor desc;
			desc.size = instanceNum * sizeof(ModelInstancingModule::PlayingAnimation);

			auto& buffer = instancing_module->getBuffer(ModelInstancingModule::PLAYING_ANIMATION);
			buffer = context->m_storage_memory.allocateMemory(desc);

			desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_playing_animation = context->m_staging_memory.allocateMemory(desc);

			auto* pa = static_cast<ModelInstancingModule::PlayingAnimation*>(staging_playing_animation.getMappedPtr());
			for (int i = 0; i < instanceNum; i++)
			{
				pa[i].playingAnimationNo = 0;
				pa[i].isLoop = true;
				pa[i].time = (float)(std::rand() % 200);
				pa[i]._p2 = 0;
			}


			vk::BufferCopy copy_info;
			copy_info.setSize(staging_playing_animation.getBufferInfo().range);
			copy_info.setSrcOffset(staging_playing_animation.getBufferInfo().offset);
			copy_info.setDstOffset(buffer.getBufferInfo().offset);
			cmd.copyBuffer(staging_playing_animation.getBufferInfo().buffer, buffer.getBufferInfo().buffer, copy_info);
		}

		instancing_module->m_instance_max_num = instanceNum;
		instancing_module->m_animation_descriptor_set = m_animation_descriptor->allocateDescriptorSet(context);
		m_animation_descriptor->updateAnimation(context, instancing_module->m_animation_descriptor_set.get(), instancing_module);

		// recode update bone & animation
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = context->m_window->getSwapchain().getBackbufferNum();
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			instancing_module->m_execute_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);

			for (size_t i = 0; i < instancing_module->m_execute_cmd.size(); i++)
			{
				auto& cmd = instancing_module->m_execute_cmd[i].get();

				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
				vk::CommandBufferInheritanceInfo inheritance_info;
				begin_info.setPInheritanceInfo(&inheritance_info);
				cmd.begin(begin_info);

				std::vector<vk::BufferMemoryBarrier> to_clear_barrier =
				{
					vk::BufferMemoryBarrier()
					.setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead)
					.setDstAccessMask(vk::AccessFlagBits::eShaderWrite)
					.setBuffer(instancing_module->getDrawIndirect().buffer)
					.setSize(instancing_module->getDrawIndirect().range)
					.setOffset(instancing_module->getDrawIndirect().offset),
				};

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eComputeShader,
					vk::DependencyFlags(), {}, to_clear_barrier, {});

				for (size_t i = 0; i < m_pipeline.size(); i++)
				{

					if (i == SHADER_COMPUTE_CULLING)
					{
						vk::BufferMemoryBarrier barrier = instancing_module->getBuffer(ModelInstancingModule::DRAW_INDIRECT).makeMemoryBarrierEx();
						barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
						barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
					}
					if (i == SHADER_COMPUTE_MOTION_UPDATE)
					{
						// 
						vk::BufferMemoryBarrier barrier = instancing_module->getBuffer(ModelInstancingModule::PLAYING_ANIMATION).makeMemoryBarrierEx();
						barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
						barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
					}

					if (i == SHADER_COMPUTE_BONE_TRANSFORM)
					{
						vk::BufferMemoryBarrier barrier = instancing_module->getBuffer(ModelInstancingModule::NODE_TRANSFORM).makeMemoryBarrierEx();
						barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
						barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
							vk::DependencyFlags(), {}, barrier, {});
					}

					cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[i].get());
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout.get(), 0, instancing_module->m_animation_descriptor_set.get(), {});
					cmd.dispatchIndirect(instancing_module->m_compute_indirect_buffer.getBufferInfo().buffer, instancing_module->m_compute_indirect_buffer.getBufferInfo().offset + i * 12);

				}
				vk::BufferMemoryBarrier to_draw_barrier = instancing_module->getBuffer(ModelInstancingModule::BONE_TRANSFORM).makeMemoryBarrierEx();
				to_draw_barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				to_draw_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, to_draw_barrier, {});
				cmd.end();

			}
		}
		return instancing_module;
	}

	enum Shader
	{
		SHADER_COMPUTE_CLEAR,
		SHADER_COMPUTE_ANIMATION_UPDATE,
		SHADER_COMPUTE_MOTION_UPDATE,
		SHADER_COMPUTE_NODE_TRANSFORM,
		SHADER_COMPUTE_CULLING,
		SHADER_COMPUTE_BONE_TRANSFORM,

		SHADER_NUM,
	};
	std::shared_ptr<ShaderModule> m_compute_shader;
	std::array<vk::UniquePipeline, SHADER_NUM> m_pipeline;
	std::shared_ptr<InstancingAnimationDescriptorModule> m_animation_descriptor;
	vk::UniquePipelineLayout m_pipeline_layout;

};

// pipeline、ライトなどの管理
struct cModelInstancingPipeline
{

	std::shared_ptr<ModelInstancingRenderPipelineComponent> m_render_pipeline;
	std::shared_ptr<ModelInstancingAnimationPipelineComponent> m_execute_pipeline;

	std::vector<std::shared_ptr<ModelInstancingModel>> m_model;

	void setup(const std::shared_ptr<btr::Context>& context)
	{
		m_render_pipeline = std::make_shared<ModelInstancingRenderPipelineComponent>(context);
		m_execute_pipeline = std::make_shared<ModelInstancingAnimationPipelineComponent>(context);
	}
	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

	void addModel(std::shared_ptr<ModelInstancingModel>& model);

	std::shared_ptr<ModelInstancingModel> createModel(std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource);

};
