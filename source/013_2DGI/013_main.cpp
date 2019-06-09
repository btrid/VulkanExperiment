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

bool intersection(vec4 aabb, vec2 pos, vec2 inv_dir, float& dist)
{
	float tx1 = ((aabb.x - pos.x)*inv_dir.x);
	float tx2 = ((aabb.z - pos.x)*inv_dir.x);

	float tmin = glm::min(tx1, tx2);
	float tmax = glm::max(tx1, tx2);

	float ty1 = ((aabb.y - pos.y)*inv_dir.y);
	float ty2 = ((aabb.w - pos.y)*inv_dir.y);

	tmin = glm::max(tmin, glm::min(ty1, ty2));
	tmax = glm::min(tmax, glm::max(ty1, ty2));

	dist = tmin;
	return tmax >= tmin;
}
int main()
{

	float dist;
	auto dir = glm::rotate(vec2(0.f, 1.f), 0.785398185f);
	auto inv_dir = 1.f/dir;
	auto pos = vec2(1023.5f, 105.5f);
	auto hit = intersection(vec4(0.5f, 0.5f, 1023.5f, 1023.5f), pos, inv_dir, dist);
	pos = pos + dir * dist;

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

// 	auto dir = normalize(vec2(-1.13, -2));
// 	auto inv_dir = vec2(1.)/dir;
// 	dir = dir * glm::min(abs(inv_dir.x), abs(inv_dir.y));
// 	inv_dir = abs(vec2(1.) / dir);
// 
// //	auto origin = vec2(10.5, 0.5);
// 	auto origin = vec2(1000.5, 1023.5);
// 	vec2 cell_origin = vec2(greaterThanEqual(dir, vec2(0.))) * vec2(8.);

	int march = 0;
// 	for (;;)
// 	{
// 		vec2 pos = glm::fma(dir, vec2(march), origin);
// 		ivec2 map_index = ivec2(pos);
// 
// 		ivec2 cell = map_index >> 3;
// 
// 		vec2 pos_sub = vec2(pos - vec2(cell << 3));
// 		vec2 tp = vec2(abs(cell_origin - pos_sub)) * inv_dir;
// 		int _axis = tp.x < tp.y ? 0 : 1;
// 		int skip = int(tp[_axis]+1.f);
// 
// 		march += skip;
// 		pos = glm::fma(dir, vec2(march), origin);
// 
// 		if(_axis == 1){ continue;}
// 
// 		march += 8-skip;
// 		pos = glm::fma(dir, vec2(march), origin);
// 
// 		if ((int(pos.y) % 8) != 7)
// 		{
// 			int a = 0;
// 			a++;
// 		}
// 
// 	}

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
	gi2d_desc.RenderWidth = app_desc.m_window_size.x;
	gi2d_desc.RenderHeight = app_desc.m_window_size.y;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		gi2d_Radiosity.executeGenerateRay(cmd);
	}

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
			device->waitIdle();
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}