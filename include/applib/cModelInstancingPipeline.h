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
#include <applib/cAppModel.h>
template<typename Set>
struct DescriptorSet
{
	vk::UniqueDescriptorSet m_handle;
	Set m_descriptors;
};

struct ModelRenderDescriptor : public DescriptorModuleOld
{
	enum {
		DESCRIPTOR_TEXTURE_NUM = 16,
	};

	struct Set
	{
		TypedBufferInfo<cModel::ModelInfo> m_model_info;
		TypedBufferInfo<ModelInstancingInfo> m_instancing_info;
		TypedBufferInfo<mat4> m_bonetransform;
		TypedBufferInfo<uint32_t> m_material_index;
		TypedBufferInfo<MaterialBuffer> m_material;
		std::array<vk::DescriptorImageInfo, DESCRIPTOR_TEXTURE_NUM> m_images;
	};

	ModelRenderDescriptor(const std::shared_ptr<btr::Context>& context)
	{
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute)
			.setDescriptorCount(DESCRIPTOR_TEXTURE_NUM)
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setBinding(5),
		};
		m_descriptor_set_layout = createDescriptorSetLayout(context, binding);
		m_descriptor_pool = createDescriptorPool(context, binding, 30);
		m_context = context;
	}

	DescriptorSet<Set> allocateDescriptorSet(Set&& set)
	{
		DescriptorSet<Set> desc;
		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(m_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		desc.m_handle = std::move(m_context->m_device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);
		vk::DescriptorBufferInfo storages[] = {
			set.m_model_info,
			set.m_instancing_info,
			set.m_bonetransform,
			set.m_material_index,
			set.m_material,
		};

		vk::WriteDescriptorSet write[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(desc.m_handle.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount((uint32_t)set.m_images.size())
			.setPImageInfo(set.m_images.data())
			.setDstBinding(5)
			.setDstSet(desc.m_handle.get()),
		};
		m_context->m_device->updateDescriptorSets(array_length(write), write, 0, 0);

		desc.m_descriptors = set;
		return desc;
	}
	std::shared_ptr<btr::Context> m_context;
};
using sModelRenderDescriptor = SingletonEx<ModelRenderDescriptor>;

struct AppModelInstancingRenderer
{
	AppModelInstancingRenderer(const std::shared_ptr<btr::Context>& context)
	{
		auto& device = context->m_device;
		m_render_pass = context->m_window->getRenderBackbufferPass();

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
				sModelRenderDescriptor::Order().getLayout(),
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
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
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
			m_pipeline = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}
	std::vector<TypedCommandBuffer<AppModelInstancingRenderer>> createCmd(const std::shared_ptr<btr::Context>& context, const Drawable* drawable, const DescriptorSet<ModelRenderDescriptor::Set>& descriptor_set)
	{
		// recode draw command
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = context->m_window->getSwapchain().getBackbufferNum();
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			auto draw_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);

			for (size_t i = 0; i < draw_cmd.size(); i++)
			{
				auto& cmd = draw_cmd[i].get();

				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
				vk::CommandBufferInheritanceInfo inheritance_info;
				inheritance_info.setFramebuffer(m_render_pass->getFramebuffer(i));
				inheritance_info.setRenderPass(m_render_pass->getRenderPass());
				begin_info.pInheritanceInfo = &inheritance_info;

				cmd.begin(begin_info);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
				std::vector<vk::DescriptorSet> sets = {
					descriptor_set.m_handle.get(),
					sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
					m_light_pipeline->getDescriptorSet(cFowardPlusPipeline::DESCRIPTOR_SET_LIGHT),
				};
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, sets, {});

				drawable->draw(cmd);

				cmd.end();
			}

			std::vector<TypedCommandBuffer<AppModelInstancingRenderer>> cmds;
			cmds.reserve(draw_cmd.size());
			for (auto&& c : draw_cmd)
			{
				cmds.emplace_back(c);
			}
			return cmds;
		}

	}
private:
	enum 
	{
		SHADER_RENDER_VERT,
		SHADER_RENDER_FRAG,
		SHADER_NUM,
	};

	std::shared_ptr<RenderPassModule> m_render_pass;
	std::shared_ptr<ShaderModule> m_shader;

	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_pipeline_layout;

	std::shared_ptr<cFowardPlusPipeline> m_light_pipeline;

};

struct InstancingAnimationDescriptorSetLayout : public DescriptorModuleOld
{
	struct Set
	{
		TypedBufferInfo<cModel::ModelInfo> m_model_info;
		TypedBufferInfo<ModelInstancingInfo> m_instancing_info;
		TypedBufferInfo<AnimationInfo> m_animation_info;
		TypedBufferInfo<PlayingAnimation> m_playing_animation;
		TypedBufferInfo<NodeInfo> m_node_info;
		TypedBufferInfo<BoneInfo> m_bone_info;
		TypedBufferInfo<mat4> m_world;
		TypedBufferInfo<mat4> m_node_transform;
		TypedBufferInfo<mat4> m_bone_transform;
		TypedBufferInfo<uint32_t> m_instance_map;
		TypedBufferInfo<uvec3> m_anime_indirect;
		TypedBufferInfo<cModel::Mesh> m_draw_indirect;

		std::vector<MotionTexture> m_motion_texture;
	};

	InstancingAnimationDescriptorSetLayout(const std::shared_ptr<btr::Context>& context)
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
		m_context = context;
	}

	DescriptorSet<Set> allocateDescriptorSet(Set&& set)
	{
		DescriptorSet<Set> desc;
		vk::DescriptorSetLayout layouts[] =
		{
			m_descriptor_set_layout.get()
		};
		vk::DescriptorSetAllocateInfo descriptor_set_alloc_info;
		descriptor_set_alloc_info.setDescriptorPool(m_descriptor_pool.get());
		descriptor_set_alloc_info.setDescriptorSetCount(array_length(layouts));
		descriptor_set_alloc_info.setPSetLayouts(layouts);
		desc.m_handle = std::move(m_context->m_device->allocateDescriptorSetsUnique(descriptor_set_alloc_info)[0]);

		vk::DescriptorBufferInfo storages[] = {
			set.m_model_info,
			set.m_instancing_info,
			set.m_animation_info,
			set.m_playing_animation,
			set.m_node_info,
			set.m_bone_info,
			set.m_node_transform,
			set.m_world,
			set.m_instance_map,
			set.m_draw_indirect,
			set.m_bone_transform,
		};

		std::vector<vk::DescriptorImageInfo> images =
		{
			vk::DescriptorImageInfo()
			.setImageView(set.m_motion_texture[0].getImageView())
			.setSampler(set.m_motion_texture[0].getSampler())
			.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
		};

		std::vector<vk::WriteDescriptorSet> write =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(desc.m_handle.get()),
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
			.setDescriptorCount(images.size())
			.setPImageInfo(images.data())
			.setDstBinding(32)
			.setDstSet(desc.m_handle.get()),
		};
		m_context->m_device->updateDescriptorSets(write, {});
		desc.m_descriptors = set;
		return desc;
	}

	std::shared_ptr<btr::Context> m_context;
};

struct ModelInstancingAnimationPipeline : public PipelineComponent
{
	ModelInstancingAnimationPipeline(const std::shared_ptr<btr::Context>& context)
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
		m_animation_descriptor_layout = std::make_shared<InstancingAnimationDescriptorSetLayout>(context);

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_animation_descriptor_layout->getLayout(),
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
	std::vector<TypedCommandBuffer<ModelInstancingAnimationPipeline>> createCmd(const std::shared_ptr<btr::Context>& context, const DescriptorSet<InstancingAnimationDescriptorSetLayout::Set>& descriptor_set)
	{
		// recode
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = context->m_window->getSwapchain().getBackbufferNum();
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			auto cmds = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);

			const auto& descriptor = descriptor_set.m_descriptors;
			for (size_t i = 0; i < cmds.size(); i++)
			{
				auto& cmd = cmds[i].get();

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
					.setBuffer(descriptor.m_draw_indirect.buffer)
					.setSize(descriptor.m_draw_indirect.range)
					.setOffset(descriptor.m_draw_indirect.offset),
				};

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eComputeShader,
					vk::DependencyFlags(), {}, to_clear_barrier, {});

				for (size_t i = 0; i < m_pipeline.size(); i++)
				{

					if (i == SHADER_COMPUTE_CULLING)
					{
						vk::BufferMemoryBarrier barrier;
						barrier.setBuffer(descriptor.m_draw_indirect.buffer);
						barrier.setSize(descriptor.m_draw_indirect.range);
						barrier.setOffset(descriptor.m_draw_indirect.offset);
						barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
						barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
					}
					if (i == SHADER_COMPUTE_MOTION_UPDATE)
					{
						// 
						vk::BufferMemoryBarrier barrier;
						barrier.setBuffer(descriptor.m_playing_animation.buffer);
						barrier.setSize(descriptor.m_playing_animation.range);
						barrier.setOffset(descriptor.m_playing_animation.offset);
						barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
						barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
					}

					if (i == SHADER_COMPUTE_BONE_TRANSFORM)
					{
						vk::BufferMemoryBarrier barrier;
						barrier.setBuffer(descriptor.m_node_transform.buffer);
						barrier.setSize(descriptor.m_node_transform.range);
						barrier.setOffset(descriptor.m_node_transform.offset);
						barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
						barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
							vk::DependencyFlags(), {}, barrier, {});
					}

					cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[i].get());
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout.get(), 0, descriptor_set.m_handle.get(), {});
					cmd.dispatchIndirect(descriptor.m_anime_indirect.buffer, descriptor.m_anime_indirect.offset + i * 12);

				}
				vk::BufferMemoryBarrier barrier;
				barrier.setBuffer(descriptor.m_bone_transform.buffer);
				barrier.setSize(descriptor.m_bone_transform.range);
				barrier.setOffset(descriptor.m_bone_transform.offset);
				barrier.setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				barrier.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, barrier, {});
				cmd.end();

			}
			std::vector<TypedCommandBuffer<ModelInstancingAnimationPipeline>> cmd;
			cmd.reserve(cmds.size());
			for (auto&& c : cmds) {
				cmd.emplace_back(c);
			}
			return cmd;
		}
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
	std::shared_ptr<InstancingAnimationDescriptorSetLayout> m_animation_descriptor_layout;
	vk::UniquePipelineLayout m_pipeline_layout;

};

// pipeline、ライトなどの管理
struct cModelInstancingPipeline
{
	std::shared_ptr<AppModelInstancingRenderer> m_render_pipeline;
	std::shared_ptr<ModelInstancingAnimationPipeline> m_execute_pipeline;

	void setup(const std::shared_ptr<btr::Context>& context)
	{
		m_render_pipeline = std::make_shared<AppModelInstancingRenderer>(context);
		m_execute_pipeline = std::make_shared<ModelInstancingAnimationPipeline>(context);
	}
};
