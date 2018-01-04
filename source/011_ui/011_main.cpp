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
#include <applib/sImGuiRenderer.h>

#include <011_ui/sUISystem.h>
#include <011_ui/UIManipulater.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


int main()
{

	btr::setResourceAppPath("..\\..\\resource\\011_ui\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 800;
	camera->getData().m_height = 600;
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

	sUISystem::Create(context);

	UIManipulater manip(context);
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			std::vector<vk::CommandBuffer> cmds(3);
			cmds[0] = manip.execute();
			cmds[1] = sUISystem::Order().draw();
			cmds[2] = sImGuiRenderer::Order().Render();
			app.submit(std::move(cmds));

// 			auto* c = context.get();
// 			auto* cp = c->m_cmd_pool.get();
// 			auto vb = c->m_vertex_memory.getBuffer();
// 			for (auto i = 0; i < 10000; i++) {
// 				auto cmd = cp->allocCmdTempolary(0);
// 				cmd->bindVertexBuffers(0, { vb }, { 0 });
// 				cmd->bindIndexBuffer(vb, 0, vk::IndexType::eUint32);
// 			}
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

