﻿#include <btrlib/Define.h>
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
#include <btrlib/Singleton.h>
#include <btrlib/sValidationLayer.h>
#include <btrlib/cWindow.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/cModelInstancingPipeline.h>
#include <applib/cAppModel.h>

#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct LightSample : public Light
{
	LightData m_param;
	int life;

	LightSample()
	{
		life = std::rand() % 50 + 30;
		m_param.m_position = glm::vec4(glm::ballRand(3000.f), std::rand() % 50 + 500.f);
		m_param.m_emission = glm::vec4(glm::normalize(glm::abs(glm::ballRand(1.f)) + glm::vec3(0.f, 0.f, 0.01f)), 1.f);

	}
	virtual bool update() override
	{
		//		life--;
		return life >= 0;
	}

	virtual LightData getParam()const override
	{
		return m_param;
	}

};


int main()
{
	btr::setResourceAppPath("..\\..\\resource\\002_model\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, -500.f, -800.f);
	camera->getData().m_target = glm::vec3(0.f, -100.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 100000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(640, 480);
	app::App app(app_desc);

	auto context = app.m_context;

	auto task = std::make_shared<std::promise<std::unique_ptr<cModel>>>();
	auto modelFuture = task->get_future();
	{
		cThreadJob job;
		auto load = [task, &context]()
		{
			auto model = std::make_unique<cModel>();
			model->load(context, btr::getResourceAppPath() + "tiny.x");
			task->set_value(std::move(model));
		};
		job.mFinish = load;
		sGlobal::Order().getThreadPool().enque(std::move(job));
	}

	while (modelFuture.valid() && modelFuture.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
		printf("wait...\n");
	}

	auto model = modelFuture.get();

	sModelRenderDescriptor::Create(context);
	sModelAnimateDescriptor::Create(context);

	std::shared_ptr<AppModel> appModel = std::make_shared<AppModel>(context, model->getResource(), 1000);
	DescriptorSet<ModelRenderDescriptor::Set> render_descriptor;
	DescriptorSet<ModelAnimateDescriptor::Set> animate_descriptor;
	{
		ModelRenderDescriptor::Set descriptor_set;
		descriptor_set.m_bonetransform = appModel->m_bone_transform_buffer.getInfoEx();
		descriptor_set.m_model_info = appModel->m_model_info_buffer.getInfoEx();
		descriptor_set.m_instancing_info = appModel->m_instancing_info_buffer.getInfoEx();
		descriptor_set.m_material = appModel->m_material.getInfoEx();
		descriptor_set.m_material_index = appModel->m_material_index.getInfoEx();
		descriptor_set.m_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
		for (size_t i = 0; i < appModel->m_texture.size(); i++)
		{
			const auto& tex = appModel->m_texture[i];
			if (tex.isReady()) {
				descriptor_set.m_images[i] = vk::DescriptorImageInfo(tex.getSampler(), tex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
			}
		}
		render_descriptor = sModelRenderDescriptor::Order().allocateDescriptorSet(std::move(descriptor_set));
	}
	{
		ModelAnimateDescriptor::Set descriptor_set;
		descriptor_set.m_model_info = appModel->m_model_info_buffer.getInfo();
		descriptor_set.m_instancing_info = appModel->m_instancing_info_buffer.getInfo();
		descriptor_set.m_node_info = appModel->m_node_info_buffer.getInfo();
		descriptor_set.m_bone_info = appModel->m_bone_info_buffer.getInfo();
		descriptor_set.m_animation_info = appModel->m_animationinfo_buffer.getInfo();
		descriptor_set.m_playing_animation = appModel->m_animationinfo_buffer.getInfo();
		descriptor_set.m_anime_indirect = appModel->m_animation_skinning_indirect_buffer.getInfo();
		descriptor_set.m_node_transform = appModel->m_node_transform_buffer.getInfo();
		descriptor_set.m_bone_transform = appModel->m_bone_transform_buffer.getInfo();
		descriptor_set.m_instance_map = appModel->m_instance_map_buffer.getInfo();
		descriptor_set.m_draw_indirect = appModel->m_draw_indirect_buffer.getInfo();
		descriptor_set.m_world = appModel->m_world_buffer.getInfo();
		descriptor_set.m_motion_texture[0].imageView = appModel->m_motion_texture[0].getImageView();
		descriptor_set.m_motion_texture[0].sampler = appModel->m_motion_texture[0].getSampler();
		descriptor_set.m_motion_texture[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		animate_descriptor = sModelAnimateDescriptor::Order().allocateDescriptorSet(std::move(descriptor_set));
	}
	appModel->m_world_buffer;
	AppModelInstancingRenderer renderer(context);
	auto drawCmd = renderer.createCmd(context, &appModel->m_resource->m_mesh_resource, render_descriptor);
	ModelInstancingAnimationPipeline animater(context);
	auto animeCmd = animater.createCmd(context, animate_descriptor);
// 	cModelInstancingPipeline pipeline;
// 	pipeline.setup(context);
// 	auto render = pipeline.createModel(context, model->getResource());
// 	pipeline.addModel(render);
// 	std::vector<ModelInstancingModule::InstanceResource> data(1000);
// 	for (int i = 0; i < 1000; i++)
// 	{
// 		data[i].m_world = glm::translate(glm::ballRand(2999.f));
// 	}
// 
// 

	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			SynchronizedPoint render_syncronized_point(1);
			SynchronizedPoint work_syncronized_point(1);

			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
// 					render->m_instancing->addModel(data.data(), data.size());
					work_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
			std::vector<vk::CommandBuffer> render_cmds(3);
			{
				MAKE_THREAD_JOB(job);
				job.mJob.emplace_back(
					[&]()
				{
// 					render_cmds[0] = pipeline.execute(context);
// 					render_cmds[1] = pipeline.m_render_pipeline->m_light_pipeline->execute(context);
// 					render_cmds[2] = pipeline.draw(context);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_syncronized_point.wait();
			app.submit(std::move(render_cmds));
			work_syncronized_point.wait();
		}

		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

