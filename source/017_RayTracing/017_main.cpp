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
#include <applib/sCameraManager.h>

#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#include <017_Raytracing/voxel.h>
#include <017_Raytracing/voxel2.h>


int main()
{
	{
		uvec2 bitmask = glm::linearRand(uvec2(1), uvec2(9999999999));

		for (int bit=0; bit < 64; bit++)
		{
			uvec2 mask = uvec2((i64vec2(1l) << clamp(i64vec2(bit+1) - i64vec2(0, 32), i64vec2(0), i64vec2(32))) - i64vec2(1l));
			uvec2 c = bitCount(bitmask & mask);
			printf("bit=%3d, count=%3d\n",bit, c.x + c.y);

//			bool isOn = (bitmask[bit/32] & (1<<bit)) != 0;
		}

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

	Voxel2 voxel(*context, *app.m_window->getFrontBuffer());
//	Voxel voxel(*context, *app.m_window->getFrontBuffer());
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		voxel.execute_MakeVoxel(cmd);
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
//					voxel.execute_MakeVoxel(cmd);
					voxel.execute_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
//					voxel.executeDebug_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
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

