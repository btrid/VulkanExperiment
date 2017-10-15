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
#include <btrlib/sValidationLayer.h>
#include <btrlib/cWindow.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/cModelInstancingPipeline.h>
#include <applib/cModelInstancingRender.h>

#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

struct LightSample : public Light
{
	LightData m_param;
	int life;

	LightSample()
	{
		life = std::rand() % 50 + 30;
		m_param.m_position = glm::vec4(glm::ballRand(3000.f), std::rand() % 50 + 500.f);
		m_param.m_emission = glm::vec4(glm::normalize(glm::abs(glm::ballRand(1.f)) + glm::vec3(0.f, 0.f, 0.01f)), 1.f);

	}
	virtual bool update() override
	{
		//		life--;
		return life >= 0;
	}

	virtual LightData getParam()const override
	{
		return m_param;
	}

};


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

	app::App app;
	{
		app::AppDescriptor desc;
		desc.m_gpu = gpu;
		desc.m_window_size = uvec2(640, 480);
		app.setup(desc);
	}

	auto context = app.m_context;

	auto task = std::make_shared<std::promise<std::unique_ptr<cModel>>>();
	auto modelFuture = task->get_future();
	{
		cThreadJob job;
		auto load = [task, &context]()
		{
			auto model = std::make_unique<cModel>();
			model->load(context, btr::getResourceAppPath() + "tiny.x");
			task->set_value(std::move(model));
		};
		job.mFinish = load;
		sGlobal::Order().getThreadPool().enque(std::move(job));
	}

	while (modelFuture.valid() && modelFuture.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
		printf("wait...\n");
	}

	auto model = modelFuture.get();


	cModelInstancingPipeline pipeline;
	pipeline.setup(context);
	auto render = pipeline.createModel(context, model->getResource());
	pipeline.addModel(render);
	std::vector<ModelInstancingModule::InstanceResource> data(1000);
	for (int i = 0; i < 1000; i++)
	{
		data[i].m_world = glm::translate(glm::ballRand(2999.f));
	}


	for (int i = 0; i < 30; i++)
	{
		pipeline.m_render_pipeline->m_light_pipeline->add(std::move(std::make_unique<LightSample>()));
	}

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
					render->m_instancing->addModel(data.data(), data.size());
					work_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			std::vector<vk::CommandBuffer> render_cmds(3);
			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[0] = pipeline.execute(context);
					render_cmds[1] = pipeline.m_render_pipeline->m_light_pipeline->execute(context);
					render_cmds[2] = pipeline.draw(context);
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
		printf("%6.4fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

