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
#include <013_2DGI/GI2D/GI2DRadiosity2.h>
#include <013_2DGI/GI2D/GI2DRadiosity3.h>
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
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;

}

int rigidbody()
{

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
 					for (int y = 0; y < 1; y++){
 					for (int x = 0; x < 1; x++){
						GI2DRB_MakeParam param;
						param.aabb = uvec4(220 + x * 32, 620 - y * 16, 16, 16);
						param.is_fluid = true;
 						gi2d_physics_context->make(cmd, param);
 					}}
				}

				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);

 				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
 				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
 				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());

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

int radiosity()
{
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
	GI2DRadiosity2 gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

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

				gi2d_Radiosity.executeRadiosity(cmd);
				gi2d_Radiosity.executeRendering(cmd);

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

struct Movable
{
	vec2 pos;
	vec2 pos_predict;
	vec2 dir;
	float scale;
	uint rb_id;
};

struct GameContext
{
	enum Layout
	{
		Layout_Map,
		Layout_Status,
		Layout_Movable,
		Layout_Num,
	};
	std::array<vk::UniqueDescriptorSetLayout, Layout_Num> m_descriptor_set_layout;
	vk::DescriptorSetLayout getDescriptorSetLayout(Layout layout)const { return m_descriptor_set_layout[layout].get(); }

	GameContext(const std::shared_ptr<btr::Context>& context)
	{
		{
			auto stage = vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout[Layout_Map] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
		}
		{
			auto stage = vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout[Layout_Status] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
		}
		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout[Layout_Movable] = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
		}
	}

	template<typename T>
	DescriptorSet<T> makeDescriptor(const std::shared_ptr<btr::Context>& context, Layout layout)
	{
		DescriptorSet<T> desc;
		vk::DescriptorSetLayout layouts[] = {
			m_descriptor_set_layout[layout].get(),
		};
		vk::DescriptorSetAllocateInfo desc_info;
		desc_info.setDescriptorPool(context->m_descriptor_pool.get());
		desc_info.setDescriptorSetCount(array_length(layouts));
		desc_info.setPSetLayouts(layouts);
		desc.m_handle = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

		desc.m_buffer = context->m_storage_memory.allocateMemory<T>({ 1,{} });
		vk::DescriptorBufferInfo storages[] = {
			desc.m_buffer.getInfo(),
		};

		vk::WriteDescriptorSet write[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(desc.m_handle.get()),
		};
		context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		return desc;
	}
};
struct GameProcedure
{
	GameProcedure(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GameContext>& game_context)
	{
		{
			const char* name[] =
			{
				"Rigid_ToFluid.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				game_context->getDescriptorSetLayout(GameContext::Layout_Movable),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayout_MovableUpdate] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

#if USE_DEBUG_REPORT
			vk::DebugUtilsObjectNameInfoEXT name_info;
			name_info.pObjectName = "PipelineLayout_MovableUpdate";
			name_info.objectType = vk::ObjectType::ePipelineLayout;
			name_info.objectHandle = reinterpret_cast<uint64_t &>(m_pipeline_layout[PipelineLayout_MovableUpdate].get());
			context->m_device->setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
#endif
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
			shader_info[0].setModule(m_shader[Shader_MovableUpdate].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_MovableUpdate].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
#if USE_DEBUG_REPORT
			vk::DebugUtilsObjectNameInfoEXT name_info;
			name_info.pObjectName = "Pipeline_MovableUpdate";
			name_info.objectType = vk::ObjectType::ePipeline;
			name_info.objectHandle = reinterpret_cast<uint64_t &>(compute_pipeline[0].get());
			context->m_device->setDebugUtilsObjectNameEXT(name_info, context->m_dispach);
#endif

			m_pipeline[Pipeline_MovableUpdate] = std::move(compute_pipeline[0]);
		}

	}

	void executeMovableUpdate(vk::CommandBuffer cmd, const std::shared_ptr<GameContext>& game_context)
	{

	}
	enum Shader
	{
		Shader_MovableUpdate,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_MovableUpdate,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MovableUpdate,
		Pipeline_Num,
	};

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
};


struct Player
{
	DescriptorSet<Movable> m_movable;

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
	return radiosity();

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
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<GI2DPhysics> gi2d_physics_context = std::make_shared<GI2DPhysics>(context, gi2d_context);
	std::shared_ptr<GameContext> game_context = std::make_shared<GameContext>(context);
	
	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity2 gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());
	GI2DPhysics_procedure gi2d_physics_proc(gi2d_physics_context, gi2d_sdf_context);

	Player player;
	player.m_movable = game_context->makeDescriptor<Movable>(context, GameContext::Layout_Movable);
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		GI2DRB_MakeParam param;
		param.aabb = uvec4(128, 128, 4, 4);
		param.is_fluid = false;
		param.is_usercontrol = true;
		gi2d_physics_context->make(cmd, param);
		auto info = player.m_movable.m_buffer.getInfo();
		info.offset += offsetof(Movable, rb_id);
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
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);

				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());

//				gi2d_Radiosity.executeRadiosity(cmd);
	//			gi2d_Radiosity.executeRendering(cmd);

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