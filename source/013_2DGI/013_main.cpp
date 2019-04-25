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
#include <013_2DGI/GI2D/GI2DSoftbody.h>

#include <013_2DGI/GI2D/GI2DRigidbody.h>
#include <013_2DGI/GI2D/GI2DPhysicsWorld.h>
#include <013_2DGI/GI2D/GI2DRigidbody_procedure.h>

#include <013_2DGI/Crowd/Crowd_Procedure.h>
#include <013_2DGI/Crowd/Crowd_CalcWorldMatrix.h>
#include <013_2DGI/Crowd/Crowd_Debug.h>

#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>

#include <013_2DGI/PathContext.h>
#include <013_2DGI/PathSolver.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
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
	GI2DDebug gi2d_debug(context, gi2d_context);
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
				gi2d_debug.executeMakeFragmentMap(cmd);

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

//				gi2d_debug.executeDrawFragmentMap(cmd, app.m_window->getFrontBuffer());
  				path_process.executeBuildTree(cmd);
				path_process.executeDrawTree(cmd, app.m_window->getFrontBuffer());

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

vec2 calcTangent(const vec2& I, const vec2& N)
{
	return I - N * dot(N, I);
}

void y(vec2 p1, vec2 p2, vec2 sdf)
{
	vec2 a = p1 + sdf - p2;
	auto c1 = dot(a, normalize(sdf)) * normalize(sdf);
	vec2 collect = p1 + sdf;
	vec2 predict = p2 + c1;

	printf("p1={%5.2f,%5.2f} p2={%5.2f,%5.2f} sdf={%5.2f,%5.2f}\n", p1.x, p1.y, p2.x, p2.y, sdf.x, sdf.y);
	printf("c ={%5.2f,%5.2f} p ={%5.2f,%5.2f}\n", collect.x, collect.y, predict.x, predict.y);
}

void y(vec2 p1, vec2 p2, vec2 sdf1, vec2 sdf2)
{
	vec2 a1 = p2 + sdf2 - p1;
	vec2 a2 = p1 + sdf1 - p2;
	auto v1 = dot(a1, normalize(sdf2)) * normalize(sdf2);
	auto v2 = dot(a2, normalize(sdf1)) * normalize(sdf1);
	vec2 pr1 = p2 + sdf2;
	vec2 pr2 = p1 + sdf1;
	vec2 c1 = p1 + v1;
	vec2 c2 = p2 + v2;

	printf("p1={%5.2f,%5.2f} p2={%5.2f,%5.2f} sdf={%5.2f,%5.2f}\n", p1.x, p1.y, p2.x, p2.y, sdf1.x, sdf1.y);
	printf("p1 ={%5.2f,%5.2f} p2 ={%5.2f,%5.2f}\n", pr1.x, pr1.y, pr2.x, pr2.y);
	printf("c2 ={%5.2f,%5.2f} c2 ={%5.2f,%5.2f}\n", c1.x, c1.y, c2.x, c2.y);
}
vec2 normalize_safe(vec2 v)
{
	return dot(v, v) == 0.f ? vec2(0.) : normalize(v);
}
int rigidbody()
{
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(-1.f, 0.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(-1.f, 0.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(-1.f, 0.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(0.f, -1.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(0.f, -1.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(2.f, 0.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(2.f, 0.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(2.f, 0.f));
// 	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(0.f, 1.f));
//	y(vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(rand() % 10 - 20, rand() % 10 - 20), vec2(0.f, 2.f));
//	y(vec2(20), vec2(20), vec2(1.f, -1.f), vec2(1.f, 1.f));
//	y(vec2(0), vec2(1, 0), vec2(0.f, 1.f), vec2(0.f, -1.f));

// 	auto xaxis = glm::rotate(vec2(1.f, 0.f), glm::radians(30.f));
// 	auto yaxis = glm::rotate(vec2(0.f, 1.f), glm::radians(30.f));
// 
// 	vec2 p1 = xaxis * vec2(0.25) + yaxis * vec2(0.25);
// 	vec2 p2 = xaxis * vec2(-0.25) + yaxis * vec2(0.25);
// 	vec2 p3 = xaxis * vec2(0.25) + yaxis * vec2(-0.25);
// 	vec2 p4 = xaxis * vec2(-0.25) + yaxis * vec2(-0.25);

//	vec2 p1 = vec2(p_x.x, p_y.x);
//	vec2 p2 = vec2(p_x.y, p_y.y);
//	vec2 p3 = vec2(p_x.z, p_y.z);
//	vec2 p4 = vec2(p_x.w, p_y.w);

// 	vec4 xaxis = glm::rotate(vec2(1.f, 0.f), glm::radians(30.f)).xyxy() * vec2(0.25, -0.25).xxyy();
// 	vec4 yaxis = glm::rotate(vec2(0.f, 1.f), glm::radians(30.f)).xyxy() * vec2(0.25, -0.25).xxyy();

	auto vel1 = vec2(0.1f, 1.f);
	auto vel2 = vec2(-0.1f, 1.f);
	auto pos1 = vec2(0.f, 1.f);
	auto pos2 = vec2(0.2f, 1.f);
	auto rela_pos = pos2 - pos1;
	auto rela_vel = vel2 - vel1;
	vec2 vel_unit = normalize_safe(rela_vel);
	vec2 pos_unit = normalize_safe(rela_pos);

	float w1 = glm::max(dot(-rela_pos, rela_vel), 0.f);

	auto rela_pos2 = pos1 - pos2;
	auto rela_vel2 = vel1 - vel2;
	float w2 = glm::max(dot(-rela_pos2, rela_vel2), 0.f);

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchainPtr());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = 1024;
	gi2d_desc.RenderHeight = 1024;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<PhysicsWorld> physics_world = std::make_shared<PhysicsWorld>(context, gi2d_context);

	GI2DClear gi2d_clear(context, gi2d_context);
	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRigidbody_procedure gi2d_rigidbody(physics_world, gi2d_sdf_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	std::shared_ptr<GI2DFluid> gi2d_Fluid = std::make_shared<GI2DFluid>(context, gi2d_context);

	physics_world->executeMakeVoronoi(cmd);

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
				if (context->m_window->getInput().m_keyboard.isOn('A'))
				{
//					physics_world->make(cmd, uvec4(255, 500, 32, 32));
					for (int y = 0; y < 20; y++)
					{
						for (int x = 0; x < 20; x++)
						{
							physics_world->make(cmd, uvec4(200 + x * 16, 570 - y * 16, 13, 13));

						}
					}
				}

				gi2d_context->execute(cmd);
				gi2d_clear.execute(cmd);
				gi2d_debug.executeMakeFragmentMap(cmd);

//				gi2d_make_hierarchy.execute(cmd);
				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
				gi2d_make_hierarchy.executeHierarchy(cmd);
				gi2d_make_hierarchy.executeMakeSDF(cmd, gi2d_sdf_context);
//				gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf_context, app.m_window->getFrontBuffer());

//				gi2d_Fluid->executeCalc(cmd);
//				gi2d_Softbody.execute(cmd);

//				gi2d_rigidbody.execute(cmd, physics_world, gi2d_sdf_context);
//				gi2d_rigidbody.executeToFragment(cmd, physics_world);

				gi2d_rigidbody.executeDrawVoronoi(cmd, physics_world);
 				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
			device->waitIdle();
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

//	return pathFinding();
	return rigidbody();

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
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
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
				gi2d_debug_make_fragment.executeMakeFragmentMap(cmd);

//#define use_sdf
#if defined(use_sdf)
				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
				gi2d_make_hierarchy.executeHierarchy(cmd);
				gi2d_make_hierarchy.executeMakeSDF(cmd, gi2d_sdf_context);
#else
				gi2d_make_hierarchy.execute(cmd);
				gi2d_make_hierarchy.executeHierarchy(cmd);
#endif
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