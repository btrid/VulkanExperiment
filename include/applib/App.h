#pragma once
#include <memory>
#include <bitset>
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

struct AppWindow : public cWindow
{
	AppWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor);

	std::vector<vk::UniqueImageView> m_backbuffer_view;

	vk::UniqueImage m_depth_image;
	vk::UniqueImageView m_depth_view;
	vk::UniqueDeviceMemory m_depth_memory;
	vk::Format m_depth_format;
	
	std::vector <vk::UniqueCommandBuffer> m_cmd_present_to_render;
	std::vector <vk::UniqueCommandBuffer> m_cmd_render_to_present;
//	PipelineFlags m_flags;

	struct ImguiRenderPipeline
	{
		ImguiRenderPipeline(const std::shared_ptr<btr::Context>& context, AppWindow* const window);
		~ImguiRenderPipeline();

		void pushImguiCmd(std::function<void()>&& imgui_cmd)
		{
			std::lock_guard<std::mutex> lg(m_cmd_mutex);
			m_imgui_cmd[sGlobal::Order().getWorkerIndex()].emplace_back(std::move(imgui_cmd));
		}
		std::vector<std::function<void()>>& getImguiCmd()
		{
			return std::move(m_imgui_cmd[sGlobal::Order().getRenderIndex()]);
		}

		std::mutex m_cmd_mutex;
		std::array<std::vector<std::function<void()>>, 2> m_imgui_cmd;
		vk::UniquePipeline m_pipeline;

		ImGuiContext* m_imgui_context;
//		std::unique_ptr < ImGuiContext, ImGui::DestroyContext > m_imgui_context;
	};
	std::unique_ptr<ImguiRenderPipeline>  m_imgui_pipeline;
	ImguiRenderPipeline* getImguiPipeline() { return m_imgui_pipeline.get(); }

};

struct RenderBackbufferAppModule : public RenderPassModule
{
	RenderBackbufferAppModule(const std::shared_ptr<btr::Context>& context, AppWindow* const window)
		: m_window(window)
	{
		auto& device = context->m_device;
		// レンダーパス
		{
			// sub pass
			std::vector<vk::AttachmentReference> color_ref =
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
			subpass.setColorAttachmentCount((uint32_t)color_ref.size());
			subpass.setPColorAttachments(color_ref.data());
			subpass.setPDepthStencilAttachment(&depth_ref);

			std::vector<vk::AttachmentDescription> attach_description = {
				// color1
				vk::AttachmentDescription()
				.setFormat(window->getSwapchain().m_surface_format.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				vk::AttachmentDescription()
				.setFormat(vk::Format::eD32Sfloat)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount((uint32_t)attach_description.size())
				.setPAttachments(attach_description.data())
				.setSubpassCount(1)
				.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}

		m_framebuffer.resize(window->getSwapchain().getBackbufferNum());
		{
			std::array<vk::ImageView, 2> view;

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount((uint32_t)view.size());
			framebuffer_info.setPAttachments(view.data());
			framebuffer_info.setWidth(window->getClientSize().x);
			framebuffer_info.setHeight(window->getClientSize().y);
			framebuffer_info.setLayers(1);

			for (size_t i = 0; i < m_framebuffer.size(); i++) {
				view[0] = window->m_backbuffer_view[i].get();
				view[1] = window->m_depth_view.get();
				m_framebuffer[i] = context->m_device->createFramebufferUnique(framebuffer_info);
			}
		}
		m_resolution = window->getClientSize<vk::Extent2D>();
	}
	virtual vk::RenderPass getRenderPass()const override { return m_render_pass.get(); }
	virtual vk::Framebuffer getFramebuffer(uint32_t index)const override { 
		return m_framebuffer[index].get(); 
	}
	virtual vk::Extent2D getResolution()const override { return m_resolution; }

private:
	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;
	vk::Extent2D m_resolution;
	AppWindow const * const m_window;
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
//	std::vector<std::shared_ptr<AppWindow>> m_window_stack;
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

