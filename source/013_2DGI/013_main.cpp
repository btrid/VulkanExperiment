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
#include <013_2DGI/GI2D/GI2DModelRender.h>
#include <013_2DGI/GI2D/GI2DRadiosity.h>
#include <013_2DGI/GI2D/GI2DFluid.h>
#include <013_2DGI/GI2D/GI2DRigidbody_dem.h>
#include <013_2DGI/GI2D/GI2DSoftbody.h>
#include <013_2DGI/Crowd/Crowd_Procedure.h>
#include <013_2DGI/Crowd/Crowd_CalcWorldMatrix.h>
#include <013_2DGI/Crowd/Crowd_Debug.h>

#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>

#include <013_2DGI/PathContext.h>
#include <013_2DGI/PathSolver.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

int pathFinding()
{
	PathContextCPU::Description desc;
	desc.m_size = ivec2(1024);
	desc.m_start = ivec2(11, 11);
	desc.m_finish = ivec2(1002, 1000);
	PathContextCPU pf(desc);
//	pf.m_field = pathmake_maze(1024*8, 1024*8);
 	pf.m_field = pathmake_noise(1024, 1024);
// 	PathSolver solver;
//	auto solve = solver.executeMakeVectorField(pf);
//	auto solve = solver.executeSolve(pf);
//	solver.writeSolvePath(pf, solve, "hoge.txt");
	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto appmodel_context = std::make_shared<AppModelContext>(context);

	cModel model;
	model.load(context, "..\\..\\resource\\tiny.x");
	std::shared_ptr<AppModel> player_model = std::make_shared<AppModel>(context, appmodel_context, model.getResource(), 128);

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchainPtr());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = 1024;
	gi2d_desc.RenderHeight = 1024;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<CrowdContext> crowd_context = std::make_shared<CrowdContext>(context, gi2d_context);
	std::shared_ptr<PathContext> path_context = std::make_shared<PathContext>(context, gi2d_context);

	GI2DClear gi2d_clear(context, gi2d_context);
	GI2DDebug gi2d_debug_make_fragment(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	Crowd_Procedure crowd_procedure(crowd_context, gi2d_context);
	Crowd_CalcWorldMatrix crowd_calc_world_matrix(crowd_context, appmodel_context);
	Crowd_Debug crowd_debug(crowd_context);

	Path_Process path_process(context, path_context, gi2d_context);

	AppModelAnimationStage animater(context, appmodel_context);
	GI2DModelRender renderer(context, appmodel_context, gi2d_context);
	auto anime_cmd = animater.createCmd(player_model);
	auto render_cmd = renderer.createCmd(player_model);

	crowd_procedure.executeMakeRay(cmd);

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
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
//				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				gi2d_context->execute(cmd);
				crowd_context->execute(cmd);
				gi2d_clear.execute(cmd);
				gi2d_debug_make_fragment.execute(cmd);

				gi2d_make_hierarchy.execute(cmd);
				gi2d_make_hierarchy.executeHierarchy(cmd);

				if (0)
				{
					crowd_debug.execute(cmd);
 					crowd_procedure.executeUpdateUnit(cmd);
 					crowd_procedure.executeMakeLinkList(cmd);
					crowd_calc_world_matrix.execute(cmd, player_model);

					std::vector<vk::CommandBuffer> anime_cmds{ anime_cmd.get() };
					animater.dispatchCmd(cmd, anime_cmds);
					std::vector<vk::CommandBuffer> render_cmds{ render_cmd.get() };
					renderer.dispatchCmd(cmd, render_cmds);
				}

				if (0)
				{
					crowd_procedure.executePathFinding(cmd);
					crowd_procedure.executeDrawField(cmd, app.m_window->getFrontBuffer());
				}

				path_process.executeBuildTree(cmd);

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;

}
int main()
{

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 512;
	camera->getData().m_height = 512;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	return pathFinding();

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto appmodel_context = std::make_shared<AppModelContext>(context);

	cModel model;
	model.load(context, "..\\..\\resource\\tiny.x");
	std::shared_ptr<AppModel> player_model = std::make_shared<AppModel>(context, appmodel_context, model.getResource(), 128);
	
	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchainPtr());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = 1024;
	gi2d_desc.RenderHeight = 1024;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<CrowdContext> crowd_context = std::make_shared<CrowdContext>(context, gi2d_context);

	GI2DClear gi2d_clear(context, gi2d_context);
	GI2DDebug gi2d_debug_make_fragment(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	gi2d_Radiosity.executeGenerateRay(cmd);
	std::shared_ptr<GI2DFluid> gi2d_Fluid = std::make_shared<GI2DFluid>(context, gi2d_context);

	Crowd_Procedure crowd_procedure(crowd_context, gi2d_context);
	Crowd_CalcWorldMatrix crowd_calc_world_matrix(crowd_context, appmodel_context);
	Crowd_Debug crowd_debug(crowd_context);
	AppModelAnimationStage animater(context, appmodel_context);
	GI2DModelRender renderer(context, appmodel_context, gi2d_context);
	auto anime_cmd = animater.createCmd(player_model);
	auto render_cmd = renderer.createCmd(player_model);

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
				cmd_crowd,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
			}

			{
//				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			// crowd
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
//				crowd_updater.execute(cmd);
				cmd.end();
				cmds[cmd_crowd] = cmd;
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				gi2d_context->execute(cmd);
				gi2d_clear.execute(cmd);
				gi2d_debug_make_fragment.execute(cmd);

// 				gi2d_Fluid->execute(cmd);
// 				gi2d_Fluid->executePost(cmd);


				gi2d_make_hierarchy.execute(cmd);
				gi2d_make_hierarchy.executeHierarchy(cmd);
				{
//					crowd_debug.execute(cmd);
// 					crowd_procedure.executeUpdateUnit(cmd);
// 					crowd_procedure.executeMakeLinkList(cmd);
//					crowd_procedure.executeMakeDensity(cmd);
//					crowd_calc_world_matrix.execute(cmd, player_model);

					std::vector<vk::CommandBuffer> anime_cmds{ anime_cmd.get() };
//					animater.dispatchCmd(cmd, anime_cmds);

				}
				{
					std::vector<vk::CommandBuffer> render_cmds{ render_cmd.get() };
//					renderer.dispatchCmd(cmd, render_cmds);
				}


//				gi2d_Fluid->executeCalc(cmd);
//				gi2d_Softbody.execute(cmd);
//				gi2d_Rigidbody.execute(cmd);
 				gi2d_Radiosity.executeRadiosity(cmd);
				gi2d_Radiosity.executeRendering(cmd);
//				crowd_procedure.executeDrawField(cmd, app.m_window->getFrontBuffer());
				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}