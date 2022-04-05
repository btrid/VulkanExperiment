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
//#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


#include <applib/sAppImGui.h>

struct Fluid
{
};
struct Particle
{
	Particle(btr::Context& ctx)
	{

	}
	void execute(vk::CommandBuffer cmd, btr::Context& ctx)
	{
		app::g_app_instance->m_window->getImgui()->pushImguiCmd([this]()
			{
				ImGui::SetNextWindowSize(ImVec2(400.f, 300.f), ImGuiCond_Once);
				static bool is_open;
				ImGui::Begin("RenderConfig", &is_open, ImGuiWindowFlags_NoSavedSettings| ImGuiWindowFlags_HorizontalScrollbar);
				if (ImGui::BeginChild("editor", ImVec2(-1, -1), false, ImGuiWindowFlags_HorizontalScrollbar)) {
					static char text[1024 * 16] =
						"/*\n"
						" The Pentium F00F bug, shorthand for F0 0F C7 C8,\n"
						" the hexadecimal encoding of one offending instruction,\n"
						" more formally, the invalid operand with locked CMPXCHG8B\n"
						" instruction bug, is a design flaw in the majority of\n"
						" Intel Pentium, Pentium MMX, and Pentium OverDrive\n"
						" processors (all in the P5 microarchitecture).\n"
						"*/\n\n"
						"label:\n"
						"\tlock cmpxchg8b eax\n";
					ImGui::InputTextMultiline("##source", text, std::size(text), ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput);
				}

				ImGui::EndChild();
				ImGui::End();
			});

	}
	void draw(vk::CommandBuffer cmd)
	{

	}

};

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\003_particle\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, -500.f, -800.f);
	camera->getData().m_target = glm::vec3(0.f, -100.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 100000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_render_target(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), context->m_window->getSwapchain());

	Particle particle(*context);
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum cmds
			{
				cmd_clear,
				cmd_particle,
				cmd_present,
				cmd_num,
			};
			std::vector<vk::CommandBuffer> render_cmds(cmd_num);
			{
				render_cmds[cmd_clear] = clear_render_target.execute();
				render_cmds[cmd_present] = present_pipeline.execute();
			}

			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

				particle.execute(cmd, *context);
				sAppImGui::Order().Render(cmd);

				cmd.end();
				render_cmds[cmd_particle] = cmd;
			}


			app.submit(std::move(render_cmds));
		}

		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

