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
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <applib/cModelPipeline.h>
#include <applib/cModelRender.h>
#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <btrlib/Loader.h>

#include <applib/sParticlePipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\003_particle\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(20.f, 10.f, 30.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::App app;
	app.setup(gpu);

	auto loader = app.m_loader;
	auto executer = app.m_executer;

	cModelPipeline model_pipeline;
	std::shared_ptr<cModelRender> model_render = std::make_shared<cModelRender>();
	cModel model;

	{
		sParticlePipeline::Order().setup(loader);

		model.load(loader, "..\\..\\resource\\tiny.x");
		model_render->setup(loader, model.getResource());
		model_pipeline.setup(loader);
		model_pipeline.addModel(executer, model_render);
		{
			PlayMotionDescriptor desc;
			desc.m_data = model.getResource()->getAnimation().m_motion[0];
			desc.m_play_no = 0;
			desc.m_start_time = 0.f;
			model_render->getMotionList().play(desc);

			auto& transform = model_render->getModelTransform();
			transform.m_local_scale = glm::vec3(0.002f);
			transform.m_local_rotate = glm::quat(1.f, 0.f, 0.f, 0.f);
			transform.m_local_translate = glm::vec3(0.f, 280.f, 0.f);
		}
	}

	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{


			{
				model_render->getModelTransform().m_global = glm::mat4(1.f);
			}

			SynchronizedPoint motion_worker_syncronized_point(1);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					model_render->work();
					motion_worker_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}

			SynchronizedPoint render_syncronized_point(2);
			std::vector<vk::CommandBuffer> render_cmds(3);
			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[0] = model_pipeline.draw(executer);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[1] = sParticlePipeline::Order().execute(executer);
					render_cmds[2] = sParticlePipeline::Order().draw(executer);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_syncronized_point.wait();

			app.submit(std::move(render_cmds));

			motion_worker_syncronized_point.wait();
		}
		app.postUpdate();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

