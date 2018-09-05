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
#include <applib/AppPipeline.h>
#include <applib/cModelPipeline.h>
#include <applib/DrawHelper.h>
#include <btrlib/Context.h>
#include <applib/Geometry.h>

#include <006_voxel_based_GI/VoxelPipeline.h>
#include <006_voxel_based_GI/ModelVoxelize.h>

#pragma comment(lib, "vulkan-1.lib")


int main()
{
	btr::setResourceAppPath("..\\..\\resource\\006_voxel_based_GI\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, -3000.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 50000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(512, 512);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_render_target(context, app.m_window->getRenderTarget());
	PresentPipeline present_pipeline(context, app.m_window->getRenderTarget(), context->m_window->getSwapchainPtr());

	VoxelInfo info;
	info.u_cell_num = uvec4(64, 16, 64, 1);
	info.u_cell_size = (info.u_area_max - info.u_area_min) / vec4(info.u_cell_num);
	std::shared_ptr<VoxelContext> vx_context = std::make_shared<VoxelContext>(context, info);
	VoxelPipeline voxelize_pipeline(context, vx_context, app.m_window->getRenderTarget());
	ModelVoxelize model_voxelize(context, vx_context);
	{
		auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			std::vector<glm::vec3> v;
			std::vector<glm::uvec3> i;
			std::tie(v, i) = Geometry::MakeSphere();

			ModelVoxelize::Model model;
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

			model_voxelize.addModel(context, setup_cmd, model);
		}
		{
			std::vector<glm::vec3> v;
			std::vector<glm::uvec3> idx;
			std::tie(v, idx) = Geometry::MakeBox(vec3(900.f));

			ModelVoxelize::Model model;
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

			model_voxelize.addModel(context, setup_cmd, model);
		}

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
				cmd_voxelize,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			SynchronizedPoint render_syncronized_point(1);

			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
					voxelize_pipeline.execute(cmd);
					model_voxelize.execute(cmd);
					voxelize_pipeline.executeMakeHierarchy(cmd);
					voxelize_pipeline.executeDraw(cmd);
					cmd.end();
					cmds[cmd_voxelize] = cmd;

					cmds[cmd_render_clear] = clear_render_target.execute();
					cmds[cmd_render_present] = present_pipeline.execute();
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_syncronized_point.wait();
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

