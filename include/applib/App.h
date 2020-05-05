#pragma once
#include <memory>
#include <bitset>
#include <mutex>
#include <functional>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCmdPool.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/Context.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Module.h>

struct ImGuiContext;


struct RenderTargetInfo
{
	ivec2 m_size;
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

	vk::UniqueDescriptorSet m_descriptor;
	btr::BufferMemoryEx<RenderTargetInfo> u_render_info;

	static vk::UniqueDescriptorSetLayout s_descriptor_set_layout;
};

struct ImguiRenderPipeline
{
	ImguiRenderPipeline(const std::shared_ptr<btr::Context>& context, struct AppWindow* const window);
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

struct AppWindow : public cWindow
{
	AppWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor);

	std::vector<vk::UniqueImageView> m_backbuffer_view;

	vk::UniqueImage m_depth_image;
	vk::UniqueImageView m_depth_view;
	vk::UniqueDeviceMemory m_depth_memory;
	vk::ImageCreateInfo m_depth_info;

	vk::ImageCreateInfo m_front_buffer_info;
	vk::UniqueImage m_front_buffer_image;
	vk::UniqueImageView m_front_buffer_view;
	vk::UniqueDeviceMemory m_front_buffer_memory;

	std::shared_ptr<RenderTarget> m_front_buffer;
	const std::shared_ptr<RenderTarget>& getFrontBuffer()const { return m_front_buffer; }

	std::unique_ptr<ImguiRenderPipeline>  m_imgui_pipeline;
	ImguiRenderPipeline* getImguiPipeline() { return m_imgui_pipeline.get(); }
};

struct AppContext : public btr::Context
{
	std::shared_ptr<AppWindow> m_app_window;
};
namespace app
{

struct AppDescriptor
{
//	 m_gpu;
	uvec2 m_window_size;
};


struct App
{
	vk::UniqueInstance m_instance;
	vk::PhysicalDevice m_physical_device;
	vk::UniqueDevice m_device;

	std::shared_ptr<cCmdPool> m_cmd_pool;
	cThreadPool m_thread_pool;
	std::shared_ptr<AppWindow> m_window; // !< mainwindow
	std::vector<std::shared_ptr<AppWindow>> m_window_list;
	std::vector<cWindowDescriptor> m_window_request;
	std::shared_ptr<btr::Context> m_context;

	std::vector<vk::CommandBuffer> m_system_cmds;
	SynchronizedPoint m_sync_point;

	std::vector<vk::UniqueFence> m_fence_list;

	vk::DispatchLoaderDynamic m_dispatch;
	vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic> m_debug_messenger;

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

