#pragma once
#include <memory>
#include <bitset>
#include <mutex>
#include <functional>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCmdPool.h>
#include <btrlib/Context.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cWindow.h>
#include <btrlib/Module.h>

struct ImGuiContext;

using PipelineFlags = std::bitset<8>;
struct PipelineFlagBits
{
	enum {
		IS_SETUP,
	};
};
struct RenderContext 
{
};
struct RenderTarget
{
	vk::ImageCreateInfo m_info;
	vk::Image m_image;
	vk::ImageView m_view;
	vk::DeviceMemory m_memory;

	vk::ImageCreateInfo m_depth_info;
	vk::Image m_depth_image;
	vk::ImageView m_depth_view;
	vk::DeviceMemory m_depth_memory;

	bool is_dynamic_resolution; //!< 可変解像度？
	vk::Extent2D m_resolution;
};

struct Cmd{};
struct PresentCmd : Cmd 
{
	std::vector<vk::UniqueCommandBuffer> cmds;
	std::vector<vk::UniqueDescriptorSet> m_copy_descriptor_set;
};

struct PresentPipeline
{
	PresentPipeline(const std::shared_ptr<btr::Context>& context, const Swapchain& swapchain)
	{
		{
			auto stage = vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setBinding(0),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

		}
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "CopyImage.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "CopyImage.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == 2, "not equal shader num");

			std::string path = btr::getResourceLibPath() + "shader\\binary\\";
			for (size_t i = 0; i < 2; i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
			}

		}

		{
			// viewの作成
			m_backbuffer_view.resize(swapchain.getBackbufferNum());
			for (uint32_t i = 0; i < m_backbuffer_view.size(); i++)
			{
				auto subresourceRange = vk::ImageSubresourceRange()
					.setAspectMask(vk::ImageAspectFlagBits::eColor)
					.setBaseArrayLayer(0)
					.setLayerCount(1)
					.setBaseMipLevel(0)
					.setLevelCount(1);

				vk::ImageViewCreateInfo backbuffer_view_info;
				backbuffer_view_info.setImage(swapchain.m_backbuffer_image[i]);
				backbuffer_view_info.setFormat(swapchain.m_surface_format.format);
				backbuffer_view_info.setViewType(vk::ImageViewType::e2D);
				backbuffer_view_info.setComponents(vk::ComponentMapping{
					vk::ComponentSwizzle::eR,
					vk::ComponentSwizzle::eG,
					vk::ComponentSwizzle::eB,
					vk::ComponentSwizzle::eA,
					});
				backbuffer_view_info.setSubresourceRange(subresourceRange);
				m_backbuffer_view[i] = context->m_device->createImageViewUnique(backbuffer_view_info);
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
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			vk::AttachmentDescription attach_description[] = 
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(swapchain.m_surface_format.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eDontCare)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}

		m_framebuffer.resize(swapchain.getBackbufferNum());
		for (uint32_t i = 0; i < m_framebuffer.size(); i++)
		{
			vk::ImageView view[] = {
				m_backbuffer_view[i].get(),
			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(1);
			framebuffer_info.setHeight(1);
			framebuffer_info.setLayers(1);

			m_framebuffer[i] = context->m_device->createFramebufferUnique(framebuffer_info);
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_layout.get()
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline 
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, swapchain.getSize().width, swapchain.getSize().height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), swapchain.getSize());
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

			vk::PipelineColorBlendAttachmentState blend_state[] = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
				.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setAlphaBlendOp(vk::BlendOp::eAdd)
				.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
				.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};

			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(array_length(blend_state));
			blend_info.setPAttachments(blend_state);

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineShaderStageCreateInfo shader_info[] = 
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[0].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[1].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			vk::PipelineDynamicStateCreateInfo dynamic_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout.get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info)
//				.setPDynamicState(&dynamic_info)
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline = std::move(pipelines[0]);
		}
	}

	std::vector<vk::UniqueImageView> m_backbuffer_view;

	vk::UniqueDescriptorSetLayout m_descriptor_layout;
	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;
	vk::UniqueShaderModule m_shader[2];

	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;

}; 
struct AppWindow : public cWindow
{
	AppWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor);

	std::vector<vk::UniqueImageView> m_backbuffer_view;

	vk::UniqueImage m_depth_image;
	vk::UniqueImageView m_depth_view;
	vk::UniqueDeviceMemory m_depth_memory;
	vk::Format m_depth_format;

	vk::ImageCreateInfo m_render_target_info;
	vk::UniqueImage m_render_target_image;
	vk::UniqueImageView m_render_target_view;
	vk::UniqueDeviceMemory m_render_target_memory;

	RenderTarget getRenderTarget()const
	{
		RenderTarget ret;
		ret.m_info = m_render_target_info;
		ret.m_image = m_render_target_image.get();
		ret.m_view = m_render_target_view.get();
		ret.m_memory = m_render_target_memory.get();
		ret.is_dynamic_resolution = false;
		ret.m_resolution.width = m_render_target_info.extent.width;
		ret.m_resolution.height = m_render_target_info.extent.height;
		return ret;
	}


	std::vector <vk::UniqueCommandBuffer> m_cmd_present_to_render;
	std::vector <vk::UniqueCommandBuffer> m_cmd_render_to_present;

	struct ImguiRenderPipeline
	{
		ImguiRenderPipeline(const std::shared_ptr<btr::Context>& context, AppWindow* const window);
		~ImguiRenderPipeline();

		void pushImguiCmd(std::function<void()>&& imgui_cmd)
		{
			std::lock_guard<std::mutex> lg(m_cmd_mutex);
			m_imgui_cmd[sGlobal::Order().getWorkerIndex()].emplace_back(std::move(imgui_cmd));
		}
		std::vector<std::function<void()>> getImguiCmd()
		{
			return std::move(m_imgui_cmd[sGlobal::Order().getRenderIndex()]);
		}

		std::mutex m_cmd_mutex;
		std::array<std::vector<std::function<void()>>, 2> m_imgui_cmd;
		vk::UniquePipeline m_pipeline;
		vk::UniqueRenderPass m_render_pass;
		vk::UniqueFramebuffer m_framebuffer;

		ImGuiContext* m_imgui_context;
	};
	std::unique_ptr<ImguiRenderPipeline>  m_imgui_pipeline;
	ImguiRenderPipeline* getImguiPipeline() { return m_imgui_pipeline.get(); }
	std::shared_ptr<PresentPipeline> m_copy_pipeline;

	PresentCmd present(const std::shared_ptr<btr::Context>& context, RenderTarget& target);
};

namespace app
{

struct AppDescriptor
{
	cGPU m_gpu;
	uvec2 m_window_size;
};
struct App
{
	cGPU m_gpu;
	std::shared_ptr<cCmdPool> m_cmd_pool;
	std::shared_ptr<AppWindow> m_window; // !< mainwindow
	std::vector<std::shared_ptr<AppWindow>> m_window_list;
	std::vector<cWindowDescriptor> m_window_request;
	std::shared_ptr<btr::Context> m_context;

	std::vector<vk::CommandBuffer> m_system_cmds;
	SynchronizedPoint m_sync_point;

	std::vector<vk::UniqueFence> m_fence_list;

	App(const AppDescriptor& desc);
	void setup();
	void submit(std::vector<vk::CommandBuffer>&& cmds);
	void preUpdate();
	void postUpdate();

	void pushWindow(const cWindowDescriptor& descriptor)
	{
		m_window_request.emplace_back(descriptor);
	}
};

extern App* g_app_instance;

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size);

}

