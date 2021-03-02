#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


int main()
{

	{
		mat4 pv[3];
		pv[0] = glm::ortho(-1024.f, 1024.f, -256.f, 256.f, 0.f, 2048.f) * glm::lookAt(vec3(0, 256, 1024), vec3(0, 256, 1024) + vec3(2048, 0, 0), vec3(0.f, 1.f, 0.f));
		pv[1] = glm::ortho(-1024.f, 1024.f, -1024.f, 1024.f, 0.f, 512.f) * glm::lookAt(vec3(1024, 0, 1024), vec3(1024, 0, 1024) + vec3(0, 512, 0), vec3(0.f, 0.f, -1.f));
		pv[2] = glm::ortho(-1024.f, 1024.f, -256.f, 256.f, 0.f, 2048.f) * glm::lookAt(vec3(1024, 256, 0), vec3(1024, 256, 0) + vec3(0, 0, 2048), vec3(0.f, 1.f, 0.f));

		vec4 t1 = pv[2] * vec4(vec3(100.f), 1.f);
		vec4 t2 = pv[2] * vec4(vec3(200.f, 100, 100.), 1.f);
		vec4 t3 = pv[2] * vec4(vec3(100.f, 200, 100.), 1.f);
		int i = 0;

	}
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-400.f, 1500.f, -1430.f);
	camera->getData().m_target = vec3(800.f, 0.f, 1000.f);
	//	camera->getData().m_position = vec3(1000.f, 220.f, -200.f);
	//	camera->getData().m_target = vec3(1000.f, 220.f, 0.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	}
	app.setup();


	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				{
				}

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

