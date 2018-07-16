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
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>

#include <applib/sImGuiRenderer.h>

#include <013_2DGI/GI2D/GI2DMakeHierarchy.h>
#include <013_2DGI/GI2D/GI2DClear.h>
#include <013_2DGI/GI2D/GI2DDebug.h>
#include <013_2DGI/GI2D/GI2DAppModel.h>
#include <013_2DGI/GI2D/GI2DRadiosity.h>

#include <013_2DGI/SV2D/SV2DRenderer.h>
#include <013_2DGI/SV2D/SV2DClear.h>
#include <013_2DGI/SV2D/SV2DLight.h>
#include <013_2DGI/SV2D/SV2DDebug.h>
#include <013_2DGI/SV2D/SV2DAppModel.h>
#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>


#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

int main()
{
#define use_pm
	using namespace gi2d;
	using namespace sv2d;
	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 512;
	camera->getData().m_height = 512;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(512, 512);
	app::App app(app_desc);

	auto context = app.m_context;

	AppModel::DescriptorSet::Create(context);
	AppModelRenderStage renderer(context, app.m_window->getRenderTarget());
	AppModelAnimationStage animater(context);

	cModel model;
	model.load(context, "..\\..\\resource\\tiny.x");
	std::shared_ptr<AppModel> player_model = std::make_shared<AppModel>(context, model.getResource(), 1);


	ClearPipeline clear_pipeline(context, app.m_window->getRenderTarget());
	PresentPipeline present_pipeline(context, app.m_window->getRenderTarget(), app.m_window->getSwapchainPtr());


#if defined(use_pm)
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context);
//	GI2DRenderer gi2d_renderer(context, app.m_window->getRenderTarget(), gi2d_context);
	GI2DMakeHierarchy gi_make_hierarchy(context, gi2d_context);
	GI2DClear gi_clear(context, gi2d_context);
	GI2DDebug gi_debug_make_fragment_and_light(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getRenderTarget());
#else
	std::shared_ptr<SV2DContext> gi2d_context = std::make_shared<SV2DContext>(context);
	SV2DRenderer gi_renderer(context, app.m_window->getRenderTarget(), gi2d_context);
	SV2DClear gi_clear(context, gi2d_context);
	SV2DDebug gi_debug_make_fragment_and_light(context, gi2d_context);
#endif

	//	SV2DAppModel pm_appmodel(context, sv2d_context);

//	auto anime_cmd = animater.createCmd(player_model);
//	auto pm_make_cmd = pm_appmodel.createCmd(player_model);

	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum 
			{
				cmd_model_update,
				cmd_render_clear,
				cmd_gi_clear,
				cmd_gi_make_fragment,
				cmd_gi_RT,
				cmd_gi_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				if(0)
				{
					auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
					{
						vk::BufferMemoryBarrier barrier = player_model->b_worlds.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, barrier, {});
					}

					auto staging = context->m_staging_memory.allocateMemory<mat4>({ 1, {} });
					*staging.getMappedPtr() = glm::translate(vec3(200.f, 0.f, 400.f)) * glm::scale(vec3(0.05f));
					vk::BufferCopy copy;
					copy.setSrcOffset(staging.getInfo().offset);
					copy.setDstOffset(player_model->b_worlds.getInfo().offset);
					copy.setSize(sizeof(mat4));

					cmd.copyBuffer(staging.getInfo().buffer, player_model->b_worlds.getInfo().buffer, copy);

					{
						vk::BufferMemoryBarrier barrier = player_model->b_worlds.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, barrier, {});
					}

//					cmd.executeCommands(1, &anime_cmd.get());

					cmd.end();
 					cmds[cmd_model_update] = cmd;
				}
			}

			{
//				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			// pm
			{
				{
					auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
					gi_clear.execute(cmd);
					gi_debug_make_fragment_and_light.execute(cmd);
					cmd.end();
					cmds[cmd_gi_clear] = cmd;
				}
				{
//					std::vector<vk::CommandBuffer> cs(1);
	//				cs[0] = pm_make_cmd.get();
		//			cmds[cmd_pm_make_fragment] = pm_appmodel.dispatchCmd(cs);
				}
				{
					auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
#if defined(use_pm)
					gi_make_hierarchy.execute(cmd);
#endif
					gi2d_Radiosity.execute(cmd);
					cmd.end();
					cmds[cmd_gi_render] = cmd;
				}
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}