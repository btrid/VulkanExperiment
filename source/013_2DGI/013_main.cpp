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

#include <applib/sImGuiRenderer.h>

#include <013_2DGI/GI2D/GI2DMakeHierarchy.h>
#include <013_2DGI/GI2D/GI2DDebug.h>
#include <013_2DGI/GI2D/GI2DModelRender.h>
#include <013_2DGI/GI2D/GI2DRadiosity.h>
#include <013_2DGI/GI2D/GI2DFluid.h>
#include <013_2DGI/GI2D/GI2DSoftbody.h>

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
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#include <taskflow/taskflow.hpp>
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
int pathFinding()
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
//  	auto solve1 = solver.executeMakeVectorField(pf);
//   	auto solve2 = solver.executeMakeVectorField2(pf);
	//	auto solve = solver.executeSolve(pf);
//	solver.writeConsole(pf);
//	solver.writeSolvePath(pf, solve, "hoge.txt");
//	solver.writeConsole(pf, solve);
//	solver.write(pf, solve2);

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto appmodel_context = std::make_shared<AppModelContext>(context);

	cModel model;
	model.load(context, "..\\..\\resource\\tiny.x");
	std::shared_ptr<AppModel> player_model = std::make_shared<AppModel>(context, appmodel_context, model.getResource(), 128);

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = 1024;
	gi2d_desc.RenderHeight = 1024;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<CrowdContext> crowd_context = std::make_shared<CrowdContext>(context, gi2d_context);
	std::shared_ptr<GI2DPathContext> gi2d_path_context = std::make_shared<GI2DPathContext>(gi2d_context);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	Crowd_Procedure crowd_procedure(crowd_context, gi2d_context);
	Crowd_CalcWorldMatrix crowd_calc_world_matrix(crowd_context, appmodel_context);
	Crowd_Debug crowd_debug(crowd_context);

	AppModelAnimationStage animater(context, appmodel_context);
	GI2DModelRender renderer(context, appmodel_context, gi2d_context);
	auto anime_cmd = animater.createCmd(player_model);
	auto render_cmd = renderer.createCmd(player_model);

	crowd_procedure.executeMakeRay(cmd);
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
//				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				crowd_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);

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
//  			path_process.executeBuildTree(cmd);
//				path_process.executeDrawTree(cmd, app.m_window->getFrontBuffer());
				gi2d_make_hierarchy.executeMakeReachMap(cmd, gi2d_path_context);
				gi2d_debug.executeDrawReachMap(cmd, gi2d_path_context, app.m_window->getFrontBuffer());

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

int rigidbody()
{

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = 1024;
	gi2d_desc.RenderHeight = 1024;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<GI2DPhysics> gi2d_physics_context = std::make_shared<GI2DPhysics>(context, gi2d_context);
	std::shared_ptr<GI2DPhysicsDebug> gi2d_physics_debug = std::make_shared<GI2DPhysicsDebug>(gi2d_physics_context, app.m_window->getFrontBuffer());

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DPhysics_procedure gi2d_physics_proc(gi2d_physics_context, gi2d_sdf_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	gi2d_debug.executeMakeFragment(cmd);
//	gi2d_physics_context->executeMakeVoronoi(cmd);
//	gi2d_physics_context->executeMakeVoronoiPath(cmd);
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
				//cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			// crowd
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				cmd.end();
				cmds[cmd_crowd] = cmd;
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				gi2d_context->execute(cmd);

				if (context->m_window->getInput().m_keyboard.isOn('A'))
				{
 					for (int y = 0; y < 16; y++){
 					for (int x = 0; x < 16; x++){
						GI2DRB_MakeParam param;
						param.aabb = uvec4(220 + x * 32, 620 - y * 16, 8, 8);
						param.is_fluid = false;
						param.is_usercontrol = false;
 						gi2d_physics_context->make(cmd, param);
 					}}
				}

				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
//				gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf_context, app.m_window->getFrontBuffer());

 				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
//				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());
				gi2d_physics_proc.executeDebugDrawCollisionHeatMap(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
			context->m_device.waitIdle();
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

int radiosity()
{
	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = app_desc.m_window_size.x;
	gi2d_desc.RenderHeight = app_desc.m_window_size.y;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				//				cmd_model_update,
				cmd_render_clear,
				//				cmd_crowd,
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
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);

				gi2d_Radiosity.executeGlobalLineRadiosity(cmd);
				gi2d_Radiosity.executeGlobalLineRadiosityRendering(cmd);

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

// Bresenham's line algorithm
// https://ja.wikipedia.org/wiki/%E3%83%96%E3%83%AC%E3%82%BC%E3%83%B3%E3%83%8F%E3%83%A0%E3%81%AE%E3%82%A2%E3%83%AB%E3%82%B4%E3%83%AA%E3%82%BA%E3%83%A0
void dda()
{
	ivec2 target(0, 256);
	ivec2 pos0(333, 333);
	ivec2 delta = abs(target - pos0);
	ivec3 _dir = glm::sign(ivec3(target, 0) - ivec3(pos0, 0));

	int axis = delta.x >= delta.y ? 0 : 1;
	ivec2 dir[2];
	dir[0] = _dir.xz();
	dir[1] = _dir.zy();

	{
		int	D = 2 * delta[1 - axis] - delta[axis];
		ivec2 pos = pos0;
		for (; true;)
		{
			if (D > 0)
			{
				pos += dir[1 - axis];
				D = D - 2 * delta[axis];
			}
			else
			{
				pos += dir[axis];
				D = D + 2 * delta[1 - axis];
			}

			printf("pos=[%3d,%3d]\n", pos.x, pos.y);
			if (all(glm::equal(pos, target)))
			{
				break;
			}
		}
		printf("pos\n");
	}
	float dist = distance(vec2(target), vec2(pos0));
	float p = 1. / (1. + dist * dist * 0.01);
	target = vec2(pos0) + vec2(target - pos0) * p * 100.;
	dist += 1.;
}

/* primitive Bresenham's-like algorithm */
void makeCircle(int Ox, int Oy, int R) 
{

	int S = 128;
	std::vector<char> field(S * S);
	int x = R;
	int y = 0;
	int err = 0;
	int dedx = (R << 1) - 1;	// 2x-1 = 2R-1
	int dedy = 1;	// 2y-1

	field[(Ox+R) + Oy * S] = 1;// +0
	field[Ox + (Oy+R) * S] = 1;// +90
	field[(Ox-R) + Oy * S] = 1;// +180
	field[Ox + (Oy-R) * S] = 1;// +270

	while (x > y) 
	{
		y++;
		err += dedy;
		dedy += 2;
		if (err >= dedx) 
		{
			x--;
			err -= dedx;
			dedx -= 2;
		}

//		assert(field[(Ox + x) + (Oy + y) * S] == 0);
		field[(Ox + x) + (Oy + y) * S] = 1;// +theta
//		assert(field[(Ox + x) + (Oy - y) * S] == 0);
		field[(Ox + x) + (Oy - y) * S] = 1;// -theta
//		assert(field[(Ox - x) + (Oy + y) * S] == 0);
		field[(Ox - x) + (Oy + y) * S] = 1;// 180-theta
//		assert(field[(Ox - x) + (Oy - y) * S] == 0);
		field[(Ox - x) + (Oy - y) * S] = 1;// 180+theta
//		assert(field[(Ox + y) + (Oy + x) * S] == 0);
		field[(Ox + y) + (Oy + x) * S] = 1;// 90+theta
//		assert(field[(Ox + y) + (Oy - x) * S] == 0);
		field[(Ox + y) + (Oy - x) * S] = 1;// 90-theta
//		assert(field[(Ox - y) + (Oy + x) * S] == 0);
		field[(Ox - y) + (Oy + x) * S] = 1;// 270+theta
//		assert(field[(Ox - y) + (Oy - x) * S] == 0);
		field[(Ox - y) + (Oy - x) * S] = 1;// 270-theta
	}

	for (int _y = 0; _y < S; _y++)
	{
		for (int _x = 0; _x < S; _x++)
		{
			printf("%c", field[_x + _y * S] ? '@' : ' ');
		}
		printf("\n");
	}
}

bool intersection() 
{
	vec4 aabb = vec4(0, 0, 1023, 1023);
	vec2 p0 = vec2(1011, 667);
	vec2 p1 = vec2(1013, 669);
	vec2 dir = vec2(p1 - p0);

// 	float tx1 = (aabb.x - p0.x) / dir.x;
// 	float tx2 = (aabb.z - p0.x) / dir.x;
// 
// 	float tmin = glm::min(tx1, tx2);
// 	float tmax = glm::max(tx1, tx2);
// 
// 	float ty1 = (aabb.y - p0.y) / dir.y;
// 	float ty2 = (aabb.w - p0.y) / dir.y;
// 
// 	tmin = glm::max(tmin, glm::min(ty1, ty2));
// 	tmax = glm::min(tmax, glm::max(ty1, ty2));
// 
// 	return tmax >= tmin;

	vec2 line[] = {
		aabb.xy() - aabb.zy(),
		aabb.zy() - aabb.zw(),
		aabb.zw() - aabb.xw(),
		aabb.xw() - aabb.xy(),
	};
	bool inner = false;
	for (int i = 0; i<4; i++ )
	{
		inner |= cross(vec3(line[i], 0.f), vec3(p0, 0.f)).z >= 0.f;
		inner |= cross(vec3(line[i], 0.f), vec3(p1, 0.f)).z >= 0.f;
	}
	return inner;
}
int radiosity2()
{
//	makeCircle(40, 40, 16);
//	dda();
//	intersection();

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = app_desc.m_window_size.x;
	gi2d_desc.RenderHeight = app_desc.m_window_size.y;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

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
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);

				gi2d_Radiosity.executeLightBasedRaytracing(cmd);

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

int radiosity3()
{
	
// 	{
// 		for (size_t i = 0; i < 32; i++)
// 		{
// //			float angle_offset = rand(vec2(gl_GlobalInvocationID.xy));
// 			float angle = i * (6.28 / 32.);
// 			vec2 dir = normalize(vec2(cos(angle), sin(angle)));
// 			printf("%i = [%6.2f%6.2f]", i, dir.x, dir.y);
// 			dir = normalize(dir);
// 			printf(", [%6.2f%6.2f]\n", dir.x, dir.y);
// 		}
// 
// 	}
	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = app_desc.m_window_size.x;
	gi2d_desc.RenderHeight = app_desc.m_window_size.y;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf = std::make_shared<GI2DSDF>(gi2d_context);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

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
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

#define USE_SDF
#if defined(USE_SDF)
				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf);
				gi2d_Radiosity.executePixelBasedRaytracing2(cmd, gi2d_sdf);
//				gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf, app.m_window->getFrontBuffer());
//				gi2d_debug.executeRenderSDF(cmd, gi2d_sdf, app.m_window->getFrontBuffer());
#else
				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);
				gi2d_Radiosity.executePixelBasedRaytracing(cmd);

#endif
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



struct Player
{
	DS_GameObject m_gameobject;
};

int main()
{

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

//	return pathFinding();
//	return rigidbody();
//	return radiosity();
	return radiosity2();
//	return radiosity3();

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());


	GI2DDescription gi2d_desc;
	gi2d_desc.RenderWidth = app_desc.m_window_size.x;
	gi2d_desc.RenderHeight = app_desc.m_window_size.y;
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