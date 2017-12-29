#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCmdPool.h>
#include <btrlib/Context.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cWindow.h>

struct AppWindow : public cWindow
{
	AppWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor)
		: cWindow(context, descriptor) 
	{}

	void pushImguiCmd(std::function<void()>&& imgui_cmd)
	{
		std::lock_guard<std::mutex> lg(m_cmd_mutex);
		m_imgui_cmd[sGlobal::Order().getCPUIndex()].emplace_back(std::move(imgui_cmd));
	}
	std::vector<std::function<void()>>& getImguiCmd()
	{
		return std::move(m_imgui_cmd[sGlobal::Order().getGPUIndex()]);
	}
	std::mutex m_cmd_mutex;
	std::array<std::vector<std::function<void()>>, 2> m_imgui_cmd;



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
	std::vector<std::shared_ptr<AppWindow>> m_window_stack;
	std::shared_ptr<btr::Context> m_context;

	std::vector<vk::CommandBuffer> m_system_cmds;
	SynchronizedPoint m_sync_point;

	std::vector<vk::UniqueFence> m_fence_list;

	App();
	void setup(const AppDescriptor& desc);
	void submit(std::vector<vk::CommandBuffer>&& cmds);
	void preUpdate();
	void postUpdate();

	void pushWindow(const std::shared_ptr<AppWindow>& window)
	{
		m_window_stack.push_back(window);
	}
};

extern App* g_app_instance;

glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size);

}

