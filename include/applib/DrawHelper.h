#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <applib/App.h>
#include <applib/Geometry.h>
#include <applib/Utility.h>
#include <applib/sCameraManager.h>
#include <applib/GraphicsResource.h>


struct DrawHelper
{

	enum DescriptorSet
	{
		DS_tex,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PL_DrawTex,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DrawTex,
		PIPELINE_DrawTexCube,
		PIPELINE_NUM,
	};


	vk::UniqueRenderPass m_renderpass;
	vk::UniqueFramebuffer m_framebuffer;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_NUM> m_DSL;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_DS;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	

	DrawHelper(std::shared_ptr<btr::Context>& ctx)
	{
		auto cmd = ctx->m_cmd_pool->allocCmdTempolary(0);

		{
			// Descriptors
			vk::DescriptorSetLayoutBinding setLayoutBinding = vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment, nullptr,  };
			vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI;
			descriptorSetLayoutCI.pBindings = &setLayoutBinding;
			descriptorSetLayoutCI.bindingCount = 1;
			descriptorSetLayoutCI.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool;
			vk::DescriptorSetLayoutBindingFlagsCreateInfo dslflagCI;
			std::vector<vk::DescriptorBindingFlags> flags = { vk::DescriptorBindingFlagBits::eUpdateAfterBind|vk::DescriptorBindingFlagBits::ePartiallyBound };
			dslflagCI.setBindingFlags(flags);
			descriptorSetLayoutCI.setPNext(&dslflagCI);
			m_DSL[DS_tex] = ctx->m_device.createDescriptorSetLayoutUnique(descriptorSetLayoutCI);

			// Descriptor sets
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DS_tex].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[DS_tex] = std::move(ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
		}

		// setup pipeline_layout
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_DSL[DS_tex].get(),
			};
			vk::PushConstantRange constant[] = {
				vk::PushConstantRange()
				.setSize(64)
				.setStageFlags(vk::ShaderStageFlagBits::eFragment)
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount((uint32_t)layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			pipeline_layout_info.setPushConstantRangeCount(array_length(constant));
			pipeline_layout_info.setPPushConstantRanges(constant);
			m_pipeline_layout[PL_DrawTex] = ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		{
			// レンダーパス
			// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			// Use subpass dependencies for layout transitions
			std::array<vk::SubpassDependency, 2> dependencies;
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

			// Create the actual renderpass
			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(RenderTarget::sformat)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eDontCare)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eUndefined)
				.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setDependencies(dependencies);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);


			m_renderpass = ctx->m_device.createRenderPassUnique(renderpass_info);

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_renderpass.get());
			framebuffer_info.setAttachmentCount(1);
			framebuffer_info.setWidth(1024);
			framebuffer_info.setHeight(1024);
			framebuffer_info.setLayers(1);
			framebuffer_info.setFlags(vk::FramebufferCreateFlagBits::eImageless);

			vk::FramebufferAttachmentImageInfo framebuffer_image_info;
			framebuffer_image_info.setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);
			framebuffer_image_info.setLayerCount(1);
			framebuffer_image_info.setViewFormatCount(1);
			framebuffer_image_info.setPViewFormats(&RenderTarget::sformat);
			framebuffer_image_info.setWidth(1024);
			framebuffer_image_info.setHeight(1024);
			vk::FramebufferAttachmentsCreateInfo framebuffer_attach_info;
			framebuffer_attach_info.setAttachmentImageInfoCount(1);
			framebuffer_attach_info.setPAttachmentImageInfos(&framebuffer_image_info);

			framebuffer_info.setPNext(&framebuffer_attach_info);

			m_framebuffer = ctx->m_device.createFramebufferUnique(framebuffer_info);

		}

		// setup pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList),
			};

			// viewport
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(nullptr);
			viewportInfo.setScissorCount(1);
			viewportInfo.setPScissors(nullptr);

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
			blend_info.setAttachmentCount(array_size(blend_state));
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo visCI;

			std::array<vk::UniqueShaderModule, 4> shader_module;
			const char* shader_name[] =
			{
				"DrawTex.vert.spv",
				"DrawTex.frag.spv",
				"DrawTexcube.vert.spv",
				"DrawTexcube.frag.spv",
			};
			std::string path = btr::getResourceLibPath() + "shader\\binary\\";
			for (size_t i = 0; i < 4; i++)
			{
				shader_module[i] = loadShaderUnique(ctx->m_device, path + shader_name[i]);
			}
			std::array<vk::PipelineShaderStageCreateInfo, 2> shaderCI_tex;
			std::array<vk::PipelineShaderStageCreateInfo, 2> shaderCI_texcube;
			shaderCI_tex[0].setModule(shader_module[0].get()).setStage(vk::ShaderStageFlagBits::eVertex).setPName("main");
			shaderCI_tex[1].setModule(shader_module[1].get()).setStage(vk::ShaderStageFlagBits::eFragment).setPName("main");
			shaderCI_texcube[0].setModule(shader_module[2].get()).setStage(vk::ShaderStageFlagBits::eVertex).setPName("main");
			shaderCI_texcube[1].setModule(shader_module[3].get()).setStage(vk::ShaderStageFlagBits::eFragment).setPName("main");

			std::array<vk::DynamicState, 2> dynamics = {
				vk::DynamicState::eViewport,
				vk::DynamicState::eScissor,
			};
			vk::PipelineDynamicStateCreateInfo dynamicStateCI;
			dynamicStateCI.setDynamicStates(dynamics);
			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shaderCI_tex))
				.setPStages(shaderCI_tex.data())
				.setPVertexInputState(&visCI)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PL_DrawTex].get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPDynamicState(&dynamicStateCI)
				.setPColorBlendState(&blend_info),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shaderCI_texcube))
				.setPStages(shaderCI_texcube.data())
				.setPVertexInputState(&visCI)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PL_DrawTex].get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPDynamicState(&dynamicStateCI)
				.setPColorBlendState(&blend_info),
			};
 			auto pipelines = ctx->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
 			if (pipelines.result == vk::Result::eSuccess)
 			{
				m_pipeline[PIPELINE_DrawTex] = std::move(pipelines.value[0]);
				m_pipeline[PIPELINE_DrawTexCube] = std::move(pipelines.value[1]);
			}

		}

	}

	vk::CommandBuffer draw(btr::Context& ctx, vk::CommandBuffer cmd, RenderTarget& rt, AppImage& img)
	{
		{
			vk::DescriptorImageInfo images[] = {
				img.info(),
			};
			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet().setDstSet(*m_DS[DS_tex]).setDstBinding(0).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(array_length(images)).setPImageInfo(images),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(256, 256)));
		begin_render_Info.setFramebuffer(m_framebuffer.get());

		vk::RenderPassAttachmentBeginInfo attachment_begin_info;
		std::array<vk::ImageView, 1> views = { rt.m_view };
		attachment_begin_info.setAttachments(views);

		begin_render_Info.pNext = &attachment_begin_info;

		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		vk::Viewport viewport{ 0,0,256,256 };
		cmd.setViewport(0, viewport);
		cmd.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(256, 256)));
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DrawTex].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PL_DrawTex].get(), 0, m_DS[DS_tex].get(), {});

		cmd.draw(4, 1, 0, 0);

		cmd.endRenderPass();

		return cmd;
	}
	vk::CommandBuffer draw_texcube(btr::Context& ctx, vk::CommandBuffer cmd, RenderTarget& rt, AppImage& img)
	{
		{
			vk::DescriptorImageInfo images[] = {
				img.info(),
			};
			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet().setDstSet(*m_DS[DS_tex]).setDstBinding(0).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(array_length(images)).setPImageInfo(images),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(1024, 256*3)));
		begin_render_Info.setFramebuffer(m_framebuffer.get());

		vk::RenderPassAttachmentBeginInfo attachment_begin_info;
		std::array<vk::ImageView, 1> views = { rt.m_view };
		attachment_begin_info.setAttachments(views);

		begin_render_Info.pNext = &attachment_begin_info;

		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PIPELINE_DrawTexCube].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PL_DrawTex].get(), 0, m_DS[DS_tex].get(), {});

		// https://www.khronos.org/opengl/wiki/Cubemap_Texture
		vk::Viewport viewport[6] =
		{ 
			{ 512,256,256,256 },//GL_TEXTURE_CUBE_MAP_POSITIVE_X
			{   0,256,256,256 },//GL_TEXTURE_CUBE_MAP_NEGATIVE_X
			{ 256,  0,256,256 },//GL_TEXTURE_CUBE_MAP_POSITIVE_Y
			{ 256,512,256,256 },//GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
			{ 256,256,256,256 },//GL_TEXTURE_CUBE_MAP_POSITIVE_Z
			{ 768,256,256,256 },//GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
		};
		for (int32_t i = 0; i < 6; i++)
		{
			cmd.setViewport(0, viewport[i]);
			cmd.setScissor(0, vk::Rect2D(vk::Offset2D((int32_t)viewport[i].x, (int32_t)viewport[i].y), vk::Extent2D(256, 256)));
			cmd.pushConstants<int32_t>(m_pipeline_layout[PL_DrawTex].get(), vk::ShaderStageFlagBits::eFragment, 0, i);

			cmd.draw(4, 1, 0, 0);
		}

		cmd.endRenderPass();

		return cmd;
	}
};