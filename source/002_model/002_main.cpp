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
#include <btrlib/AllocatedMemory.h>
#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>

#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\002_model\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, -500.f, -800.f);
	camera->getData().m_target = glm::vec3(0.f, -100.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 100000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(640, 480);
	app::App app(app_desc);

	auto context = app.m_context;

	auto task = std::make_shared<std::promise<std::unique_ptr<cModel>>>();
	auto modelFuture = task->get_future();
	{
		cThreadJob job;
		auto load = [task, &context]()
		{
			auto model = std::make_unique<cModel>();
			model->load(context, btr::getResourceAppPath() + "../tiny.x");
			task->set_value(std::move(model));
		};
		job.mFinish = load;
		sGlobal::Order().getThreadPool().enque(std::move(job));
	}

	while (modelFuture.valid() && modelFuture.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
		printf("wait...\n");
	}

	auto model = modelFuture.get();
	AppModel::DescriptorSet::Create(context);
	AppModelRenderStage renderer(context, app.m_window->getRenderTarget());
	AppModelAnimationStage animater(context);

	std::shared_ptr<AppModel> appModel = std::make_shared<AppModel>(context, model->getResource(), 1000);

	auto drawCmd = renderer.createCmd(appModel);
	auto animeCmd = animater.createCmd(appModel);

	ClearPipeline clear_render_target(context, app.m_window->getRenderTarget());
	PresentPipeline present_pipeline(context, app.m_window->getRenderTarget(), context->m_window->getSwapchainPtr());
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			SynchronizedPoint render_syncronized_point(1);
			SynchronizedPoint work_syncronized_point(1);

			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
// 					render->m_instancing->addModel(data.data(), data.size());
					work_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			std::vector<vk::CommandBuffer> render_cmds(5);
			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[0] = clear_render_target.execute();
					render_cmds[4] = present_pipeline.execute();
					{
						std::vector<vk::CommandBuffer> cmds(1);
						cmds[0] = animeCmd.get();
						render_cmds[1] = animater.dispach(cmds);
					}
					{
						render_cmds[2] = renderer.getLight()->execute(context);
					}
					{
						std::vector<vk::CommandBuffer> cmds(1);
						cmds[0] = drawCmd.get();
						render_cmds[3] = renderer.draw(cmds);
					}
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_syncronized_point.wait();
			app.submit(std::move(render_cmds));
			work_syncronized_point.wait();
		}

		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

