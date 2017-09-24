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
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <applib/cModelPipeline.h>
#include <applib/cModelRender.h>
#include <applib/DrawHelper.h>
#include <btrlib/Loader.h>
#include <applib/Geometry.h>
// #define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
// #include <imgui/imgui.h>

#include <006_voxel_based_GI/VoxelPipeline.h>
#include <006_voxel_based_GI/ModelVoxelize.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
//#pragma comment(lib, "imgui.lib")


int main()
{
	btr::setResourceAppPath("..\\..\\resource\\006_voxel_based_GI\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, -3000.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 50000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::App app;
	app.setup(gpu);

	auto loader = app.m_loader;
	auto executer = app.m_executer;

	VoxelPipeline voxelize_pipeline;
	voxelize_pipeline.setup(loader);
	{
		auto model_voxelize = voxelize_pipeline.createPipeline<ModelVoxelize>(loader);
		auto setup_cmd = loader->m_cmd_pool->allocCmdTempolary(0);
		{
			std::vector<glm::vec3> v;
			std::vector<glm::uvec3> i;
			std::tie(v, i) = Geometry::MakeSphere();

			VoxelizeModel model;
			model.m_mesh.resize(1);
			model.m_mesh[0].vertex.resize(v.size());
			for (size_t i = 0; i < model.m_mesh[0].vertex.size(); i++)
			{
				model.m_mesh[0].vertex[i].pos = v[i] * 200.f;
			}
			model.m_mesh[0].index = i;
			model.m_mesh[0].m_material_index = 0;

			model.m_material[0].albedo = glm::vec4(1.f, 0.f, 0.f, 1.f);
			model.m_material[0].emission = glm::vec4(0.f, 0.f, 0.f, 1.f);

			model_voxelize->addModel(executer, setup_cmd.get(), model);
		}
		{
			std::vector<glm::vec3> v;
			std::vector<glm::uvec3> idx;
			std::tie(v, idx) = Geometry::MakeBox(900.f);

			VoxelizeModel model;
			model.m_mesh.resize(5);
			model.m_mesh[0].vertex.resize(v.size());
			for (size_t i = 0; i < model.m_mesh.size(); i++)
			{
				model.m_mesh[i].vertex.resize(v.size());
				for (size_t j = 0; j < v.size(); j++)
				{
					model.m_mesh[i].vertex[j].pos = v[j];
				}
				model.m_mesh[i].index.push_back(idx[i * 2]);
				model.m_mesh[i].index.push_back(idx[i * 2 + 1]);
				model.m_mesh[i].m_material_index = i;
			}
			model.m_material[0].albedo = glm::vec4(0.4f, 0.4f, 1.f, 1.f);
			model.m_material[1].albedo = glm::vec4(1.f, 0.4f, 0.4f, 1.f);
			model.m_material[2].albedo = glm::vec4(0.4f, 1.f, 0.4f, 1.f);
			model.m_material[3].albedo = glm::vec4(1.f, 0.4f, 0.4f, 1.f);
			model.m_material[4].albedo = glm::vec4(1.f, 1.f, 1.0f, 1.f);
			model.m_material[5].albedo = glm::vec4(1.f, 0.f, 0.4f, 1.f);

			model_voxelize->addModel(executer, setup_cmd.get(), model);
		}

	}

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			{
// 				DrawCommand dcmd;
// 				dcmd.world = glm::scale(vec3(200.f));
// 				DrawHelper::Order().drawOrder(DrawHelper::SPHERE, dcmd);
			}

			SynchronizedPoint render_syncronized_point(1);
			std::vector<vk::CommandBuffer> render_cmds(1);

			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[0] = voxelize_pipeline.draw(executer);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_syncronized_point.wait();
			app.submit(std::move(render_cmds));
		}
		app.postUpdate();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

