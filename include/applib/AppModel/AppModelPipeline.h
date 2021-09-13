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
#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>
#include <applib/sCameraManager.h>

struct AppModelAnimationStage
{
	AppModelAnimationStage(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<AppModelContext>& appmodel_context)
	{
		m_context = context;
		m_appmodel_context = appmodel_context;
		std::string path = btr::getResourceShaderPath();
		std::vector<ShaderDescriptor> shader_desc =
		{
			{ path + "001_Clear.comp.spv", vk::ShaderStageFlagBits::eCompute },
			{ path + "002_AnimationUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path + "003_MotionUpdate.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path + "004_NodeTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path + "005_CameraCulling.comp.spv",vk::ShaderStageFlagBits::eCompute },
			{ path + "006_BoneTransform.comp.spv",vk::ShaderStageFlagBits::eCompute },
		};
		assert(shader_desc.size() == SHADER_NUM);
		m_compute_shader = std::make_shared<ShaderModule>(context, shader_desc);

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				appmodel_context->getLayout(AppModelContext::DescriptorLayout_Model),
				appmodel_context->getLayout(AppModelContext::DescriptorLayout_Update),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info = vk::PipelineLayoutCreateInfo()
				.setSetLayoutCount(array_length(layouts))
				.setPSetLayouts(layouts);
			m_pipeline_layout = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
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

		for (int i = 0; i < compute_pipeline_info.size(); i++)
		{
			auto p = context->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[i]);
 			m_pipeline[i] = std::move(p.value);
		}


	}
	vk::UniqueCommandBuffer createCmd(const std::shared_ptr<AppModel>& render)
	{
		// recode
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.commandPool = m_context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
		auto cmd = std::move(m_context->m_device.allocateCommandBuffersUnique(cmd_buffer_info)[0]);


		{
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
			vk::CommandBufferInheritanceInfo inheritance_info;
			begin_info.setPInheritanceInfo(&inheritance_info);
			cmd->begin(begin_info);

			vk::DescriptorSet descriptors[] = {
				render->getDescriptorSet(AppModel::DescriptorSet_Model),
				render->getDescriptorSet(AppModel::DescriptorSet_Update),
			};
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout.get(), 0, array_length(descriptors), descriptors, 0, {});

			auto offset = sizeof(uvec3);
			// SHADER_COMPUTE_CLEAR
			{
				vk::BufferMemoryBarrier barrier = render->b_draw_indirect.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead, vk::AccessFlagBits::eShaderWrite);
				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});

				cmd->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[SHADER_COMPUTE_CLEAR].get());
				cmd->dispatchIndirect(render->b_animation_indirect.getInfo().buffer, render->b_animation_indirect.getInfo().offset + SHADER_COMPUTE_CLEAR*offset);
			}
			// SHADER_COMPUTE_ANIMATION_UPDATE
			{
// 				vk::BufferMemoryBarrier barrier[] = {
// 					render->b_animation_work.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
// 					render->b_node_transform.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite)
// 				};
// 				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(barrier), barrier, 0, nullptr);

				cmd->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[SHADER_COMPUTE_ANIMATION_UPDATE].get());
				cmd->dispatchIndirect(render->b_animation_indirect.getInfo().buffer, render->b_animation_indirect.getInfo().offset + SHADER_COMPUTE_ANIMATION_UPDATE * offset);
			}

			// SHADER_COMPUTE_MOTION_UPDATE
			{
				vk::BufferMemoryBarrier barrier[] = {
					render->b_animation_work.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					render->b_node_transform.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite)
				};
				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(barrier), barrier, 0, nullptr);

				cmd->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[SHADER_COMPUTE_MOTION_UPDATE].get());
				cmd->dispatchIndirect(render->b_animation_indirect.getInfo().buffer, render->b_animation_indirect.getInfo().offset + SHADER_COMPUTE_MOTION_UPDATE * offset);
			}
			// SHADER_COMPUTE_NODE_TRANSFORM
			{
				vk::BufferMemoryBarrier barrier[] = {
					render->b_node_transform.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					render->b_instance_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(barrier), barrier, 0, nullptr);

				cmd->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[SHADER_COMPUTE_NODE_TRANSFORM].get());
				cmd->dispatchIndirect(render->b_animation_indirect.getInfo().buffer, render->b_animation_indirect.getInfo().offset + SHADER_COMPUTE_NODE_TRANSFORM * offset);
			}
			// SHADER_COMPUTE_CULLING
			{
				vk::BufferMemoryBarrier barrier = render->b_draw_indirect.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});

				cmd->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[SHADER_COMPUTE_CULLING].get());
				cmd->dispatchIndirect(render->b_animation_indirect.getInfo().buffer, render->b_animation_indirect.getInfo().offset + SHADER_COMPUTE_CULLING * offset);
			}
			// SHADER_COMPUTE_BONE_TRANSFORM
			{
				vk::BufferMemoryBarrier barrier[] = 
				{
					render->b_node_transform.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					render->b_instance_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					render->b_model_instancing_info.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
					vk::DependencyFlags(), 0, nullptr, array_length(barrier), barrier, 0, nullptr);

				cmd->bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[SHADER_COMPUTE_BONE_TRANSFORM].get());
				cmd->dispatchIndirect(render->b_animation_indirect.getInfo().buffer, render->b_animation_indirect.getInfo().offset + SHADER_COMPUTE_BONE_TRANSFORM * offset);
			}

			vk::BufferMemoryBarrier barrier[] = {
				render->b_bone_transform.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				render->b_draw_indirect.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
			};
			cmd->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect|vk::PipelineStageFlagBits::eVertexShader, {}, 0, {}, array_length(barrier), barrier, 0, {});
			cmd->end();
		}
		return cmd;
	}

	void dispatchCmd(vk::CommandBuffer cmd, std::vector<vk::CommandBuffer>& cmds)
	{
		cmd.executeCommands(cmds.size(), cmds.data());
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
	vk::UniquePipelineLayout m_pipeline_layout;

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<AppModelContext> m_appmodel_context;
};

struct AppModelRenderStage
{
	AppModelRenderStage(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<AppModelContext>& appmodel_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		auto& device = context->m_device;
		m_context = context;
		m_appmodel_context = appmodel_context;
		m_light_pipeline = std::make_shared<cFowardPlusPipeline>();
		m_light_pipeline->setup(context);
		{
			for (int i = 0; i < 30; i++)
			{
//				m_light_pipeline->add(std::move(std::make_unique<LightSample>()));
			}
		}

		// レンダーパス
		{
			// sub pass
			vk::AttachmentReference color_ref[] =
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
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);
			subpass.setPDepthStencilAttachment(&depth_ref);

			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(render_target->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				// depth
				vk::AttachmentDescription()
				.setFormat(render_target->m_depth_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);
		}

		{
			vk::ImageView view[] = {
				render_target->m_view,
				render_target->m_depth_view,
			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(render_target->m_info.extent.width);
			framebuffer_info.setHeight(render_target->m_info.extent.height);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
		}
		// setup shader
		{
			std::string path = btr::getResourceShaderPath();
			std::vector<ShaderDescriptor> shader_desc =
			{
				{ path+"Render.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ path+ "Render.frag.spv",vk::ShaderStageFlagBits::eFragment },
//				{ path+ "RenderFowardPlus.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			assert(shader_desc.size() == SHADER_NUM);
			m_shader = std::make_shared<ShaderModule>(context, shader_desc);
		}


		{
			vk::DescriptorSetLayout layouts[] = 
			{
				appmodel_context->getLayout(AppModelContext::DescriptorLayout_Model),
				appmodel_context->getLayout(AppModelContext::DescriptorLayout_Render),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				m_light_pipeline->getDescriptorSetLayout(cFowardPlusPipeline::DESCRIPTOR_SET_LAYOUT_LIGHT),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			m_pipeline_layout = device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			vk::Extent3D size = render_target->m_info.extent;
			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height));

			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(1);
			viewport_info.setPViewports(&viewport);
			viewport_info.setScissorCount(1);
			viewport_info.setPScissors(&scissor);

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
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline = std::move(device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info).value[0]);
		}
		m_render_target = render_target;
	}
	vk::UniqueCommandBuffer createCmd(const std::shared_ptr<AppModel>& render)
	{
		// recode draw command
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.commandPool = m_context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
		auto cmd = std::move(m_context->m_device.allocateCommandBuffersUnique(cmd_buffer_info)[0]);
		{

			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
			vk::CommandBufferInheritanceInfo inheritance_info;
			inheritance_info.setFramebuffer(m_framebuffer.get());
			inheritance_info.setRenderPass(m_render_pass.get());
			begin_info.pInheritanceInfo = &inheritance_info;

			cmd->begin(begin_info);

			cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
			std::vector<vk::DescriptorSet> sets = {
				render->getDescriptorSet(AppModel::DescriptorSet_Model),
				render->getDescriptorSet(AppModel::DescriptorSet_Render),
				sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA),
				m_light_pipeline->getDescriptorSet(cFowardPlusPipeline::DESCRIPTOR_SET_LIGHT),
			};
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, sets, {});

			render->m_render.draw(cmd.get());

			cmd->end();
		}
		return cmd;

	}

	void dispach(vk::CommandBuffer cmd, std::vector<vk::CommandBuffer>& cmds)
	{
		vk::RenderPassBeginInfo render_begin_info;
		render_begin_info.setRenderPass(m_render_pass.get());
		render_begin_info.setFramebuffer(m_framebuffer.get());
		render_begin_info.setRenderArea(vk::Rect2D({}, m_render_target->m_resolution));

		cmd.beginRenderPass(render_begin_info, vk::SubpassContents::eSecondaryCommandBuffers);
		cmd.executeCommands(cmds.size(), cmds.data());
		cmd.endRenderPass();
	}

private:
	enum 
	{
		SHADER_RENDER_VERT,
		SHADER_RENDER_FRAG,
		SHADER_NUM,
	};

	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;
	std::shared_ptr<ShaderModule> m_shader;

	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_pipeline_layout;

	std::shared_ptr<cFowardPlusPipeline> m_light_pipeline;
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<AppModelContext> m_appmodel_context;
 public:
	std::shared_ptr<cFowardPlusPipeline> getLight() { return m_light_pipeline; }

};
