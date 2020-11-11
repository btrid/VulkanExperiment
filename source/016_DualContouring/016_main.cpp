#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
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

#include <applib/sAppImGui.h>


#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

int main()
{
	// 	for(;;)
	// 	{ 
	//		double a = glm::linearRand(UINT_MAX - 3, UINT_MAX) / double(UINT_MAX);
	// 		float a = glm::linearRand((UINT_MAX&0x7fffff) - 3, (UINT_MAX & 0x7fffff)) / float(UINT_MAX & 0x7fffff);
	// 		printf("%36.24f\n", a);
	// 	}

	// 	for (;;)
	// 	{
	// 		float a = glm::linearRand(-glm::pi<float>(), glm::pi<float>());
	// 		float b = glm::linearRand(-glm::pi<float>(), glm::pi<float>());
	// 		float d = b - a;
	// 		d = d > 3.14f ? 6.28f - d : d;
	// 		d = d < -3.14f ? 6.28f + d : d;
	// 		printf("a=%8.3f, b=%8.3f, d=%8.3f\n", a, b, d);
	// 	}

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;


	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());



	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

// 		cmd.updateBuffer<uint64_t>(game_context->b_state.getInfo().buffer, game_context->b_state.getInfo().offset, 1ull);
// 		cmd.updateBuffer<vec2>(game_context->b_movable.getInfo().buffer, game_context->b_movable.getInfo().offset, vec2(756.f, 990.f));
// 
// 		GI2DRB_MakeParam param;
// 		param.aabb = uvec4(756, 990, 16, 16);
// 		param.is_fluid = false;
// 		param.is_usercontrol = true;
// 		gi2d_physics_context->make(cmd, param);
// 		auto info = game_context->b_movable.getInfo();
// 		info.offset += player.m_gameobject.index * sizeof(cMovable) + offsetof(cMovable, rb_address);
// 		gi2d_physics_context->getRBID(cmd, info);
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
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}
}