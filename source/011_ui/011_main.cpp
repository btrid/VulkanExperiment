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
#include <applib/sAppImGuiRenderer.h>

#include <011_ui/sUISystem.h>
#include <011_ui/UIManipulater.h>
#include <011_ui/Font.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
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

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(1200, 800);
	app::App app(app_desc);

	auto context = app.m_context;

	sUISystem::Create(context, app.m_window->getRenderTarget());
	UIManipulater manip(context);

	sFont::Create(context);
	sUIManipulater::Create(context);

	PipelineDescription p_desc;
	p_desc.m_context = context;
	p_desc.m_render_target = app.m_window->getRenderTarget();
	FontRenderer font_renderer(context, p_desc);
	FontDescription font_desc;
	font_desc.filename = "mgenplus-1c-black.ttf";
	font_desc.m_glyph_size = uvec2(36, 36);
	Font font(font_desc);

	GlyphCacheDescription cache_desc;
	cache_desc.m_glyph_num = 64;
	auto cache = font.makeCache(context, cache_desc);

	TextRequest request;
	request.m_area_size = uvec2(100);
	request.m_text = U"Windowsでコンピューターの世界が広がります。1234567890.:,;'\"(!?)+-*/=";
	request.m_vertical = false;
	auto text_data = font.makeRender(context, request, cache);
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			std::vector<vk::CommandBuffer> cmds(4);
			SynchronizedPoint sync_point(3);
			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
//					/*cmds[0] =*/ manip.execute();
//					serialize(manip);
					auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
					sUIManipulater::Order().execute(cmd);
					cmd.end();
					cmds[0] = cmd;
					sync_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
					cmds[1] = sUISystem::Order().draw();
					sync_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
//					cmds[2] = sImGuiRenderer::Order().Render();
					sync_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			cmds[3] = context->m_cmd_pool->allocCmdOnetime(0);
//			font_renderer.draw(cmds[3], font, cache, text_data);
			cmds[3].end();
			sync_point.wait();
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

