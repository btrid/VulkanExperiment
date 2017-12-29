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
#include <applib/DrawHelper.h>
#include <btrlib/Context.h>
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>


#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


int main()
{
	btr::setResourceAppPath("..\\..\\resource\\007_multi_window\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(220.f, 60.f, 300.f);
	camera->getData().m_target = glm::vec3(220.f, 20.f, 201.f);
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
		desc.m_window_size = uvec2(1200, 800);
		app.setup(desc);
	}
	auto context = app.m_context;
	{
		cWindowDescriptor window_info;
		window_info.class_name = L"Sub Window";
		window_info.window_name = L"Sub Window";
		window_info.backbuffer_num = sGlobal::Order().FRAME_MAX;
		window_info.size = vk::Extent2D(480, 480);
		window_info.surface_format_request = app.m_window->getSwapchain().m_surface_format;
		auto sub = sWindow::Order().createWindow<AppWindow>(context, window_info);
		app.m_window_list.push_back(sub);
	}


	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			app.submit(std::vector<vk::CommandBuffer>{});
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

