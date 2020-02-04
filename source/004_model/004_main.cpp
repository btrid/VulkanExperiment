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
#include <btrlib/Singleton.h>
#include <btrlib/cWindow.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/cModel.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>
#include <applib/AppPipeline.h>

#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, -500.f, 800.f);
	camera->getData().m_target = glm::vec3(0.f, -100.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);
	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	auto appmodel_context = std::make_shared<AppModelContext>(context);
	AppModelRenderStage model_renderer(context, appmodel_context, app.m_window->getFrontBuffer());
	AppModelAnimationStage model_animater(context, appmodel_context);

	cModel model;
	model.load(context, btr::getResourceAppPath() + "tiny.x");
//	model.load2(context, btr::getResourceAppPath() + "tiny.x");
	std::shared_ptr<AppModel> player_model = std::make_shared<AppModel>(context, appmodel_context, model.getResource(), 1024);

	auto anime_cmd = model_animater.createCmd(player_model);
	auto render_cmd = model_renderer.createCmd(player_model);

	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_model_update,
				cmd_model_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				std::vector<vk::CommandBuffer> anime_cmds = { anime_cmd.get() };
				model_animater.dispatchCmd(cmd, anime_cmds);

				cmd.end();
				cmds[cmd_model_update] = cmd;

			}
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				std::vector<vk::CommandBuffer> render_cmds = { render_cmd.get() };
				model_renderer.dispach(cmd, render_cmds);
				cmd.end();
				cmds[cmd_model_render] = cmd;

			}
			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			app.submit(std::move(cmds));
		}

		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

