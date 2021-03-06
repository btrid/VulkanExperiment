#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
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

#include <applib/sAppImGui.h>

#include <013_2DGI/GI2D/GI2DMakeHierarchy.h>
#include <013_2DGI/GI2D/GI2DDebug.h>
#include <013_2DGI/GI2D/GI2DModelRender.h>
#include <013_2DGI/GI2D/GI2DRadiosity.h>
#include <013_2DGI/GI2D/GI2DFluid.h>
#include <013_2DGI/GI2D/GI2DSoftbody.h>
#include <013_2DGI/GI2D/GI2DPath.h>

#include <013_2DGI/GI2D/GI2DPhysics.h>
#include <013_2DGI/GI2D/GI2DPhysics_procedure.h>
#include <013_2DGI/GI2D/GI2DVoronoi.h>

#include <013_2DGI/Crowd/Crowd_Procedure.h>
#include <013_2DGI/Crowd/Crowd_CalcWorldMatrix.h>
#include <013_2DGI/Crowd/Crowd_Debug.h>

#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>

#include <013_2DGI/PathContext.h>
#include <013_2DGI/PathSolver.h>


#include <013_2DGI/Game/Game.h>
#include <013_2DGI/Game/Movable.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct Bullet 
{
};
struct Collision
{
	std::shared_ptr<GI2DContext> m_ctx_gi2d;
	std::shared_ptr<CrowdContext> m_ctx_crowd;

};
PathContextCPU pathmake_file()
{
	PathContextCPU::Description desc;
	desc.m_size = ivec2(64);
//	desc.m_start = ivec2(11, 11);
//	desc.m_finish = ivec2(1002, 1000);

	auto aa = ivec2(64, 64) >> 3;
	std::vector<uint64_t> data(aa.x*aa.y);

	FILE* f = nullptr;
	fopen_s(&f, "..\\..\\resource\\testmap.txt", "r");
	char b[64];
	for (int y = 0; y < 64; y++)
	{
		fread_s(b, 64, 1, 64, f);
		for (int x = 0; x < 64; x++)
		{
			switch (b[x]) 
			{
			case '-':
				break;
			case 'X':
				data[x/8 + y/8 * aa.x] |= 1ull << (x%8 + (y%8)*8);
				break;
			case '#':
				desc.m_start = ivec2(x, y);
				break;
			case '*':
				desc.m_finish = ivec2(x, y);
				break;
			}
		}

	}
	PathContextCPU pf(desc);
	pf.m_field = data;
	return pf;
}

int crowd()
{

	PathContextCPU::Description desc;
	desc.m_size = ivec2(1024);
	desc.m_start = ivec2(11, 11);
	desc.m_finish = ivec2(1002, 1000);
	PathContextCPU pf(desc);
//	pf.m_field = pathmake_maze(1024, 1024);
	pf.m_field = pathmake_noise(1024, 1024);
//	pf = pathmake_file();
	PathSolver solver;
	auto solve = solver.execute_solve_by_jps(pf);
	solver.writefile_solvebyjps(pf, solve);

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

//	auto appmodel_context = std::make_shared<AppModelContext>(context);

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = uvec2(1024);
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<CrowdContext> crowd_context = std::make_shared<CrowdContext>(context, gi2d_context);
	std::shared_ptr<GI2DPathContext> gi2d_path_context = std::make_shared<GI2DPathContext>(gi2d_context);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	Crowd_Procedure crowd_procedure(crowd_context, gi2d_context, gi2d_sdf_context, gi2d_path_context);
//	Crowd_CalcWorldMatrix crowd_calc_world_matrix(crowd_context, appmodel_context);
	Crowd_Debug crowd_debug(crowd_context);


	gi2d_debug.executeUpdateMap(cmd, pf.m_field),
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
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				crowd_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);
				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
//				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);

				crowd_procedure.executeUpdateUnit(cmd);

				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				crowd_procedure.executeRendering(cmd, app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		context->m_device.waitIdle();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;

}



struct Player
{
	DS_GameObject m_gameobject;
};

int main()
{
// 	for(;;)
// 	{ 
//		double a = glm::linearRand(UINT_MAX - 3, UINT_MAX) / double(UINT_MAX);
// 		float a = glm::linearRand((UINT_MAX&0x7fffff) - 3, (UINT_MAX & 0x7fffff)) / float(UINT_MAX & 0x7fffff);
// 		printf("%36.24f\n", a);
// 	}

// 	for (;;)
// 	{
// 		float a = glm::linearRand(-glm::pi<float>(), glm::pi<float>());
// 		float b = glm::linearRand(-glm::pi<float>(), glm::pi<float>());
// 		float d = b - a;
// 		d = d > 3.14f ? 6.28f - d : d;
// 		d = d < -3.14f ? 6.28f + d : d;
// 		printf("a=%8.3f, b=%8.3f, d=%8.3f\n", a, b, d);
// 	}

//	dda_test();

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	return crowd();
//	return gi2d_path::path_finding();
//	return rigidbody();
//	return radiosity_globalline();
//	return radiosity_rightbased();
//	return radiosity_pixelbased();

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());


	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = app_desc.m_window_size;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<GI2DPhysics> gi2d_physics_context = std::make_shared<GI2DPhysics>(context, gi2d_context);
	std::shared_ptr<GameContext> game_context = std::make_shared<GameContext>(context, gi2d_context, gi2d_physics_context);
	
	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());
	GI2DPhysics_procedure gi2d_physics_proc(gi2d_physics_context, gi2d_sdf_context);
	GameProcedure game_proc(context, game_context);
	Player player;
	{		
		player.m_gameobject = game_context->alloc();

	}

	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		cmd.updateBuffer<uint64_t>(game_context->b_state.getInfo().buffer, game_context->b_state.getInfo().offset, 1ull);
		cmd.updateBuffer<vec2>(game_context->b_movable.getInfo().buffer, game_context->b_movable.getInfo().offset, vec2(756.f, 990.f));

		GI2DRB_MakeParam param;
		param.aabb = uvec4(756, 990, 16, 16);
		param.is_fluid = false;
		param.is_usercontrol = true;
		gi2d_physics_context->make(cmd, param);
		auto info = game_context->b_movable.getInfo();
		info.offset += player.m_gameobject.index * sizeof(cMovable) + offsetof(cMovable, rb_address);
		gi2d_physics_context->getRBID(cmd, info);
	}

	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				if (context->m_window->getInput().m_keyboard.isOn('A'))
				{
					for (int y = 0; y < 1; y++) {
						for (int x = 0; x < 1; x++) {
							GI2DRB_MakeParam param;
							param.aabb = uvec4(880 + x * 32, 620 - y * 16, 64, 64);
//							param.aabb = uvec4(880 + x * 32, 620 - y * 16, 8, 8);
							param.is_fluid = false;
							param.is_usercontrol = false;
							gi2d_physics_context->make(cmd, param);
						}
					}
				}


				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
 				game_proc.executePlayerUpdate(cmd, context, game_context, player.m_gameobject);
 				game_proc.executeMovableUpdatePrePyhsics(cmd, context, game_context, player.m_gameobject);
 				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
 				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());
				game_proc.executeMovableUpdatePostPyhsics(cmd, context, game_context, player.m_gameobject);

//				gi2d_debug.executeDrawFragmentMap(cmd, app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}
}