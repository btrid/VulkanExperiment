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
#include <btrlib/Context.h>
#include <010_sound/rWave.h>
#include <010_sound/sSoundSystem.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
//#pragma comment(lib, "imgui.lib")


int main()
{
	btr::setResourceAppPath("..\\..\\resource\\010_sound\\");

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::App app;
	{
		app::AppDescriptor desc;
		desc.m_gpu = gpu;
		desc.m_window_size = uvec2(640, 480);
		app.setup(desc);
	}

	auto context = app.m_context;

	sSoundSystem::Order().setup(context);

	std::shared_ptr<rWave> wave = std::make_shared<rWave>("..\\..\\resource\\010_sound\\Alesis-Fusion-Steel-String-Guitar-C4.wav");
	std::vector<std::shared_ptr<SoundBuffer>> bank = {
		std::make_shared<SoundBuffer>(context, wave),
	};
	sSoundSystem::Order().setSoundbank(context, std::move(bank));
//	sSoundSystem::Order().playOneShot(0);

	while (true)
	{
		cStopWatch time;

		app.preUpdate();

		{
			cThreadJob job;
			job.mFinish = [=]() { sSoundSystem::Order().execute_loop(context); };
//			sGlobal::Order().getThreadPoolSound().enque(std::move(job));
			sGlobal::Order().getThreadPool().enque(std::move(job));
		}
		{
			app.submit(std::vector<vk::CommandBuffer>{});
		}
		app.postUpdate();
		printf("%6.4fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

