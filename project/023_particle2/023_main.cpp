#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <btrlib/cWindow.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#include <applib/sAppImGui.h>

//template<typename N, typename T>
struct RenderNode
{
	//	constexpr size_t name() { std::hash<N>; }
	//	constexpr size_t hash() { std::hash<std::string>(N); }
};
struct RenderGraph
{
	std::unordered_map<std::string, RenderNode> m_graph;

	template<typename N, typename T, typename... Args>
	constexpr T Add(Args... args)
	{
		return m_graph[N].emplace(args);
	}
};

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\003_particle\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(-0.3f, 0.0f, -0.3f);
	camera->getData().m_target = glm::vec3(1.f, 0.0f, 1.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	ClearPipeline clear_render_target(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), context->m_window->getSwapchain());

	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);

	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum cmds
			{
				cmd_clear,
				cmd_particle,
				cmd_present,
				cmd_num,
			};
			std::vector<vk::CommandBuffer> render_cmds(cmd_num);
			{
				render_cmds[cmd_clear] = clear_render_target.execute();
				render_cmds[cmd_present] = present_pipeline.execute();
			}

			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

				sAppImGui::Order().Render(cmd);
				cmd.end();
				render_cmds[cmd_particle] = cmd;
			}


			app.submit(std::move(render_cmds));
		}

		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

}

