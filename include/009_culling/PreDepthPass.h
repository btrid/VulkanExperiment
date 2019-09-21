#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/Singleton.h>
#include <applib/sCameraManager.h>

struct RenderModule
{
	virtual void draw(vk::CommandBuffer cmd) = 0;
};

struct IDepthRender
{
//	virtual DescriptorSet<InstancingDescriptorSet> m_desc;
};
struct ModelRenderer : Singleton<ModelRenderer>
{
	friend Singleton<ModelRenderer>;

	// setup descriptor_set_layout
	void setup(const std::shared_ptr<btr::Context>& context)
	{
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		std::vector<vk::DescriptorSetLayoutBinding> binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
		};

		DescriptorLayoutDescriptor desc;
		desc.binding = binding;
		desc.m_set_num = 20;
		m_model_descriptor_layout = std::make_shared<DescriptorLayoutEx>(context, desc);
	}
	std::shared_ptr<DescriptorLayoutEx> m_model_descriptor_layout;

};
struct DepthRenderPass : public RenderPassModule
{
	DepthRenderPass(const std::shared_ptr<btr::Context>& context)
	{
		auto& device = context->m_device;
		// レンダーパス
		{
			// sub pass
			vk::AttachmentReference depth_ref;
			depth_ref.setAttachment(0);
			depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setPDepthStencilAttachment(&depth_ref);

			vk::AttachmentDescription attach_description[] = {
				vk::AttachmentDescription()
				.setFormat(context->m_window->getSwapchain().m_depth.m_format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);

			m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);
		}
		{
			vk::ImageView view[] = {
				context->m_window->getSwapchain().m_depth.m_view,
			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(context->m_window->getClientSize().x);
			framebuffer_info.setHeight(context->m_window->getClientSize().y);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
		}
	}
	vk::RenderPass getRenderPass()const override { return m_render_pass.get(); }
	vk::Framebuffer getFramebuffer(uint32_t index)const override { return m_framebuffer.get(); }

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

};
struct DepthRenderer 
{
};

struct PreDepthPass;
struct PreDepthPipeline : public std::enable_shared_from_this<PreDepthPipeline>
{
	PreDepthPipeline(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PreDepthPass>& pass)
		: m_pass(pass)
	{
		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "WriteDepth.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "WriteDepth.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.get(), path + shader_info[i].name);
				m_shader_info[i].setModule(m_shader_module[i].get());
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		// setup pipeline_layout
		{
			std::vector<vk::DescriptorSetLayout> layouts =
			{
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				ModelRenderer::Order().m_model_descriptor_layout->getLayout(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		{

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList),
			};

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_TRUE)
				.setSrcColorBlendFactor(vk::BlendFactor::eOne)
				.setDstColorBlendFactor(vk::BlendFactor::eDstAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo no_blend_info;

			vk::VertexInputAttributeDescription attr[] =
			{
				vk::VertexInputAttributeDescription().setLocation(0).setOffset(0).setBinding(0).setFormat(vk::Format::eR32G32B32A32Sfloat)
			};
			vk::VertexInputBindingDescription binding[] =
			{
				vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(12)
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexAttributeDescriptionCount(array_length(attr));
			vertex_input_info.setPVertexAttributeDescriptions(attr);
			vertex_input_info.setVertexBindingDescriptionCount(array_length(binding));
			vertex_input_info.setPVertexBindingDescriptions(binding);


			// setup pipeline
			vk::Extent3D size;
			size.setWidth(context->m_window->getSwapchain().m_backbuffer[0].m_size.width);
			size.setHeight(context->m_window->getSwapchain().m_backbuffer[0].m_size.height);
			size.setDepth(1);

			// viewport
			vk::Viewport viewport[] = { vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f) };
			vk::Rect2D scissor[] = { vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height)) };
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				m_shader_info[SHADER_VERT_WRITE_DEPTH],
				m_shader_info[SHADER_FRAG_WRITE_DEPTH],
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout.get())
				.setRenderPass(m_pass->m_render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&no_blend_info),
			};
			auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
			m_pipeline = std::move(pipelines[0]);

		}
	}
	
	void draw(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
	}

	enum Shader
	{
		SHADER_VERT_WRITE_DEPTH,
		SHADER_FRAG_WRITE_DEPTH,
		SHADER_NUM,
	};

	std::shared_ptr<PreDepthPass> m_pass;
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	template<typename T, typename... Args>
	std::shared_ptr<T> makeRenderer(const std::shared_ptr<btr::Context>& context, Args... args)
	{
		struct U : public T
		{
			template<typename... Args>
			U(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PreDepthPipeline>& parent, Args... args)
				: T(context, parent, args...)
			{}
		};
		auto parent = shared_from_this();
		auto ptr = std::make_shared<U>(context, parent, args...);
//		m_renderer.push_back(ptr);
		return ptr;
	}

//	std::vector<std::shared_ptr<Renderer>> m_renderer;

};
struct PreDepthPass : public std::enable_shared_from_this<PreDepthPass>
{
	PreDepthPass(const std::shared_ptr<btr::Context>& context) 
	{
		m_render_pass = std::make_shared<DepthRenderPass>(context);
	}

	template<typename T, typename... Args>
	std::shared_ptr<T> makePipeline(const std::shared_ptr<btr::Context>& context, Args... args)
	{
		struct U : public T
		{
			template<typename... Args> 
			U(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PreDepthPass>& pass, Args... args)
				: T(context, pass, args...)
			{}
		};
		auto pass = shared_from_this();
		auto ptr = std::make_shared<U>(context, pass, args...);
		m_pipelines.push_back(ptr);
		return ptr;
	}

	vk::CommandBuffer draw(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass->getRenderPass());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), context->m_window->getClientSize<vk::Extent2D>()));
		begin_render_Info.setFramebuffer(m_render_pass->getFramebuffer(0));
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		for (auto& pipeline : m_pipelines)
		{
			pipeline->draw(context, cmd);
		}

		cmd.endRenderPass();
		cmd.end();
		return cmd;

	}
	std::shared_ptr<DepthRenderPass> m_render_pass;
	std::vector<std::shared_ptr<PreDepthPipeline>> m_pipelines;
};