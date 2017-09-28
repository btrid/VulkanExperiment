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
#include <btrlib/cModel.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/cModelRender.h>
#include <applib/cModelRenderPrivate.h>
#include <applib/cModelPipeline.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\004_model\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(10.f, 5.f, 20.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;


	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::App app;
	app.setup(gpu);

	auto context = app.m_context;

	cModel model;
	model.load(context, btr::getResourceAppPath() + "tiny.x");


	std::shared_ptr<cModelRender> render = std::make_shared<cModelRender>();
	render->setup(context, model.getResource());
	{
		PlayMotionDescriptor desc;
		desc.m_data = model.getResource()->getAnimation().m_motion[0];
		desc.m_play_no = 0;
		desc.m_start_time = 0.f;
		render->getMotionList().play(desc);

		auto& transform = render->getModelTransform();
		transform.m_local_scale = glm::vec3(0.02f);
		transform.m_local_rotate = glm::quat(1.f, 0.f, 0.f, 0.f);
		transform.m_local_translate = glm::vec3(0.f, 0.f, 0.f);
	}


	cModelPipeline renderer;
	renderer.setup(context);
	renderer.addModel(context, render);
 
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			render->getModelTransform().m_global = mat4(1.f);
			render->work();

			std::vector<vk::CommandBuffer> render_cmds(1);
			render_cmds[0] = renderer.draw(context);
			app.submit(std::move(render_cmds));
		}
		app.postUpdate();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

