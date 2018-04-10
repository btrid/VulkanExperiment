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
#include <applib/sImGuiRenderer.h>

#include <013_2DGI/PM2DRenderer.h>
#include <013_2DGI/PM2DRenderer2.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

int main()
{
	vec2 pos = vec2(0.7f, 0.2f);
	vec2 dir = glm::normalize(vec2(-0.5, -0.7f));
	vec2 next_step = vec2(-1.f, -1.f);
	vec2 inv_dir = 1.f / dir;
	ivec2 map_index = ivec2(3, 5);
	int hierarchy = 0;
//	for (;;)
	{
		vec2 cell_size = vec2(1.f);
		vec2 cell_origin = vec2(1.f)*cell_size;
		vec2 map_index_sub = map_index - ((map_index >> hierarchy) << hierarchy);
		vec2 cell_p = abs(cell_origin - (pos + map_index_sub));

		vec2 progress = abs(cell_p * inv_dir);
		int next_axis = progress.x < progress.y ? 0 : progress.x > progress.y ? 1 : 2;

		vec2 absmove = abs(dir * progress[next_axis % 2]);
		vec2 next = floor(absmove + pos)*next_step.xy;
		map_index += ivec2(next);
		pos = fract(absmove + pos);
	}

//	using namespace pm2d_2;
	using namespace pm2d;
	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 512;
	camera->getData().m_height = 512;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(512, 512);
	app::App app(app_desc);

	auto context = app.m_context;
	ClearPipeline clear_pipeline(context, app.m_window->getRenderTarget());
	PresentPipeline present_pipeline(context, app.m_window->getRenderTarget(), app.m_window->getSwapchainPtr());

	std::shared_ptr<PM2DRenderer> pm_renderer = std::make_shared<PM2DRenderer>(context, app.m_window->getRenderTarget());
	DebugPM2D pm_debug(context, pm_renderer);
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum 
			{
				cmd_clear,
				cmd_pm,
				cmd_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			cmds[cmd_clear] = clear_pipeline.execute();
			auto p = std::vector<PM2DPipeline*>{&pm_debug};
			cmds[cmd_pm] = pm_renderer->execute(p);
			cmds[cmd_present] = present_pipeline.execute();
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}