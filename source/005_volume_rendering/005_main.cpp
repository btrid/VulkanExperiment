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
#include <atomic>
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
#include <applib/sCameraManager.h>

#include <btrlib/Context.h>
#include <005_volume_rendering/VolumeRenderer.h>
#include <applib/DrawHelper.h>


#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\005_volume_rendering\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, -4000.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 201.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 50000.f;
	camera->getData().m_near = 0.01f;
	auto gpu = sGlobal::Order().getGPU(0);
	auto device = gpu.getDevice();

	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);

	app::App app;
	app.setup(gpu);

	auto context = app.m_context;

	volumeRenderer volume_renderer;
	volume_renderer.setup(context);

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{

			// draw
			SynchronizedPoint worker_syncronized_point(1);
			{
				cThreadJob job;
				job.mFinish = 
				[&]()
				{
					volume_renderer.work(context);
					worker_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);

			}
			SynchronizedPoint render_syncronized_point(1);
			std::vector<vk::CommandBuffer> render_cmds(1);
			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]() 
					{
						render_cmds[0] = volume_renderer.draw(context);
						render_syncronized_point.arrive();
					}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			render_syncronized_point.wait();

			app.submit(std::move(render_cmds));

			worker_syncronized_point.wait();
		}

		app.postUpdate();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

