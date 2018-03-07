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

	bool is_dynamic_resolution; //!< ‰Â•Ï‰ð‘œ“xH
	vk::Extent2D m_resolution;
};

struct AppWindow : public cWindow
{
	AppWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor);

	std::vector<vk::UniqueImageView> m_backbuffer_view;

	vk::UniqueImage m_depth_image;
	vk::UniqueImageView m_depth_view;
	vk::UniqueDeviceMemory m_depth_memory;
	vk::ImageCreateInfo m_depth_info;

	vk::ImageCreateInfo m_render_target_info;
	vk::UniqueImage m_render_target_image;
	vk::UniqueImageView m_render_target_view;
	vk::UniqueDeviceMemory m_render_target_memory;

	std::shared_ptr<RenderTarget> m_render_target;
	const std::shared_ptr<RenderTarget>& getRenderTarget()const { return m_render_target; }

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

