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

#include <013_2DGI/GI2D/GI2DRigidbody.h>
#include <013_2DGI/GI2D/GI2DPhysics.h>
#include <013_2DGI/GI2D/GI2DPhysics_procedure.h>

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

#define VMA_IMPLEMENTATION
#include <VulkanMemoryAllocator/src/vk_mem_alloc.h>

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
	std::shared_ptr<GI2DPathContext> gi2d_path_context = std::make_shared<GI2DPathContext>(gi2d_context);

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
//  				path_process.executeBuildTree(cmd);
//				path_process.executeDrawTree(cmd, app.m_window->getFrontBuffer());
				gi2d_make_hierarchy.executeMakeReachMap(cmd, gi2d_path_context);
				gi2d_debug.executeDrawReachMap(cmd, gi2d_path_context, app.m_window->getFrontBuffer());

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

int rigidbody()
{

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

// 	VmaAllocatorCreateInfo vma_allocater_info = {};
// 	vma_allocater_info.physicalDevice = (VkPhysicalDevice)gpu.getHandle();
// 	vma_allocater_info.device = (VkDevice)device.getHandle();
// 	vma_allocater_info.flags = 0;
// 	vma_allocater_info.pVulkanFunctions = nullptr;
// 	vma_allocater_info.pAllocationCallbacks = nullptr;
// 	vma_allocater_info.pDeviceMemoryCallbacks = nullptr;
// 	vma_allocater_info.frameInUseCount = 0;
// 	vma_allocater_info.pHeapSizeLimit = nullptr;
// 	vma_allocater_info.preferredLargeHeapBlockSize = 0;
// 	vma_allocater_info.pRecordSettings = nullptr;
// 
// 	VmaAllocator allocater;
// 	vmaCreateAllocator(&vma_allocater_info, &allocater);

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
	std::shared_ptr<GI2DPathContext> gi2d_path_context = std::make_shared<GI2DPathContext>(gi2d_context);
	std::shared_ptr<GI2DPhysics> gi2d_physics_context = std::make_shared<GI2DPhysics>(context, gi2d_context);
	std::shared_ptr<GI2DPhysicsDebug> gi2d_physics_debug = std::make_shared<GI2DPhysicsDebug>(gi2d_physics_context, app.m_window->getFrontBuffer());

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DPhysics_procedure gi2d_physics_proc(gi2d_physics_context, gi2d_sdf_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	gi2d_debug.executeMakeFragment(cmd);
	gi2d_physics_context->executeMakeVoronoi(cmd);
	gi2d_physics_context->executeMakeVoronoiPath(cmd);
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
				//				crowd_updater.execute(cmd);
				cmd.end();
				cmds[cmd_crowd] = cmd;
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				gi2d_context->execute(cmd);

				if (context->m_window->getInput().m_keyboard.isOn('A'))
				{
// 					for (int y = 0; y < 1; y++){
// 					for (int x = 0; x < 1; x++){
// 						gi2d_physics_context->make(cmd, uvec4(200 + x * 32, 620 - y * 16, 16, 16));
// 					}}
 					gi2d_physics_context->executeDestructWall(cmd);
				}


				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
				gi2d_make_hierarchy.executeHierarchy(cmd);
				gi2d_make_hierarchy.executeMakeSDF(cmd, gi2d_sdf_context);
//				gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf_context, app.m_window->getFrontBuffer());

//				gi2d_make_hierarchy.executeMakeReachMap(cmd, gi2d_path_context);
//				gi2d_debug.executeDrawReachMap(cmd, gi2d_path_context, app.m_window->getFrontBuffer());

 				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
 				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
 				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());

//				physics_world->executeMakeVoronoi(cmd);
//				if (app.m_window->getInput().m_keyboard.isHold('A'))
				{
//					gi2d_rigidbody.executeDrawVoronoi(cmd, physics_world);
//					gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				}
//				else 
				{
//					physics_debug->executeDrawVoronoiTriangle(cmd);
//					physics_world->executeMakeVoronoiPath(cmd);
//					physics_debug->executeDrawVoronoiPath(cmd);
				}
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

#define map_reso 1024
#define dir_reso_bit (7)
#define dir_reso (128)
ivec2 cell_origin = (ivec2(8) << dir_reso_bit);
bool intersection(ivec2 pos, ivec2 dir, int& n, int& f)
{
	ivec4 aabb = ivec4(0, 0, map_reso, map_reso)*dir_reso;
	pos *= dir_reso;
	ivec4 t = ((aabb - pos.xyxy)/dir.xyxy());
	t += ivec4(notEqual((aabb - pos.xyxy) % dir.xyxy(), ivec4(0)));

	ivec2 tmin = glm::min(t.xy(), t.zw());
	ivec2 tmax = glm::max(t.xy(), t.zw());

	n = glm::max(tmin.x, tmin.y);
	f = glm::min(tmax.x, tmax.y);

	return glm::min(tmax.x, tmax.y) > glm::max(glm::max(tmin.x, tmin.y), 0);
}


vec2 rotateEx(float angle)
{
	float c = cos(angle);
	float s = sin(angle);
	return vec2(c, s);
}

void calcDirEx(float angle, vec2& dir, vec2& inv_dir)
{
	dir = rotateEx(angle);
	dir.x = abs(dir.x) < FLT_EPSILON ? (dir.x >= 0. ? 0.0001 : -0.0001) : dir.x;
	dir.y = abs(dir.y) < FLT_EPSILON ? (dir.y >= 0. ? 0.0001 : -0.0001) : dir.y;
	inv_dir = 1.f / dir;
	dir = dir * glm::min(abs(inv_dir.x), abs(inv_dir.y));
	inv_dir = 1.f / dir;
}
#define HALF_PI glm::radians(90.)
#define TWO_PI glm::radians(360.)
#define PI glm::radians(180.)
#define QUARTER_PI glm::radians(45.)

void test2()
{
	std::vector<int> map(map_reso * map_reso);

	int angle_num = 32;

	for (int angle_index = 0; angle_index < angle_num; angle_index++)
	{
		uint area = angle_index / (angle_num);
		float a = HALF_PI / (angle_num);
		float angle = glm::fma(a, float(angle_index), a*0.5f);

		vec2 dir, inv_dir;
		calcDirEx(angle, dir, inv_dir);

		auto i_dir = abs(ivec2(round(dir * dir_reso)));
		auto i_inv_dir = abs(ivec2(round(inv_dir * dir_reso)));
		printf("dir[%3d]=[%4d,%4d]\n", angle_index, i_dir.x, i_dir.y);

		for (int x = 0; x < map_reso * 2; x++)
		{
			ivec2 origin = ivec2(0);
			if (i_dir.x>= i_dir.y)
			{
				origin.y += (map_reso-1) - int((ceil(i_dir.y / (float)i_dir.x))) * x;
			}
			else
			{
 				origin.x += (map_reso - 1) - int((ceil(i_dir.x / (float)i_dir.y))) * x;
			}

			int begin, end;
			if (!intersection(origin, i_dir, begin, end))
			{
				break;
			}

			for (int i = begin; i < end;)
			{
// 				ivec2 t1 = i * i_dir.xy() + origin * dir_reso;
// 				ivec2 mi = t1 >> dir_reso_bit;
// 
// 				map[mi.x + mi.y*map_reso] += 1;
// 				ivec2 next = ((mi >> 3) + 1) << 3;
// 				ivec2 tp = next - mi;
// 
// 				i++;

				ivec2 space = cell_origin - ((i * i_dir.xy + origin* dir_reso) % cell_origin);
				ivec2 tp = space / i_dir;
				int skip = (int(glm::min(tp.x, tp.y))) + 1;
				i += skip;
			}

		}

// 		for (int y = 0; y < map_reso; y++)
// 		{
// 			for (int x = 0; x < map_reso; x++)
// 			{
// 				assert(map[x + y * map_reso] == angle_index+1);
// 			}
// 		}
	}
}
int main()
{
//	test2();
	
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
//	return rigidbody();

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
	gi2d_desc.RenderWidth = app_desc.m_window_size.x;
	gi2d_desc.RenderHeight = app_desc.m_window_size.y;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

//	Crowd_Procedure crowd_procedure(crowd_context, gi2d_context);
//	Crowd_CalcWorldMatrix crowd_calc_world_matrix(crowd_context, appmodel_context);
//	Crowd_Debug crowd_debug(crowd_context);
//	AppModelAnimationStage animater(context, appmodel_context);
//	GI2DModelRender renderer(context, appmodel_context, gi2d_context);
//	auto anime_cmd = animater.createCmd(player_model);
//	auto render_cmd = renderer.createCmd(player_model);

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
			}

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			// crowd
			{
//				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
//				crowd_updater.execute(cmd);
//				cmd.end();
//				cmds[cmd_crowd] = cmd;
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);
				gi2d_make_hierarchy.executeHierarchy(cmd);

				gi2d_Radiosity.executeRadiosity(cmd);
				gi2d_Radiosity.executeRendering(cmd);

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
//			device->waitIdle();
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}