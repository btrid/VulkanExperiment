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
#include <applib/DrawHelper.h>
#include <btrlib/Context.h>
#include <btrlib/VoxelPipeline.h>

#include <999_game/sBulletSystem.h>
#include <999_game/sBoid.h>
#include <999_game/sCollisionSystem.h>
#include <999_game/sScene.h>
#include <999_game/MapVoxelize.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

struct Gun{

	virtual ~Gun() = default;

	void shoot()
	{

	}
};
struct Player
{
	glm::vec3 m_pos; //!< 位置
	glm::vec3 m_dir; //!< 向き
	glm::vec3 m_inertia;	//!< 移動方向
	uint32_t m_state;		//!< 状態

	Gun m_left;
	Gun m_right;


	void execute(std::shared_ptr<btr::Context>& executer)
	{
		const cInput& input = executer->m_window->getInput();
		auto resolution = executer->m_window->getClientSize();
		auto mouse_n1_to_1 = (glm::vec2(input.m_mouse.xy) / glm::vec2(resolution) - 0.5f) * 2.f;
		if (glm::dot(mouse_n1_to_1, mouse_n1_to_1) >= 0.0001f) {
			m_dir = glm::normalize(-glm::vec3(mouse_n1_to_1.x, 0.f, mouse_n1_to_1.y));
		}

		m_inertia *= pow(0.85f, 1.f + sGlobal::Order().getDeltaTime());
		m_inertia.x = glm::abs(m_inertia.x) < 0.001f ? 0.f : m_inertia.x;
		m_inertia.z = glm::abs(m_inertia.z) < 0.001f ? 0.f : m_inertia.z;

		bool is_left = input.m_keyboard.isHold('a')|| input.m_keyboard.isHold(VK_LEFT);
		bool is_right = input.m_keyboard.isHold('d')|| input.m_keyboard.isHold(VK_RIGHT);
		bool is_front = input.m_keyboard.isHold('w')|| input.m_keyboard.isHold(VK_UP);
		bool is_back = input.m_keyboard.isHold('s') || input.m_keyboard.isHold(VK_DOWN);
		glm::vec3 inertia = glm::vec3(0.f);
		inertia.z += is_front * 5.f;
		inertia.z -= is_back * 5.f;
		inertia.x += is_right * 5.f;
		inertia.x -= is_left * 5.f;
		m_inertia += inertia;
		m_inertia = glm::clamp(m_inertia, glm::vec3(-1.f), glm::vec3(1.f));
		m_pos += m_inertia * 0.01f;
		
		{
			std::vector<BulletData> m_bullet;
			if (input.m_keyboard.isHold('j'))
			{
				BulletData b;
				b.m_life = 1.f;
				b.m_pos = glm::vec4(m_pos, 0.3f);
				b.m_vel = glm::vec4(m_dir, 1.f) * 33.3f;
				b.m_type = 0;
				b.m_map_index = sScene::Order().calcMapIndex(b.m_pos);
				m_bullet.push_back(b);
			}
			if (input.m_keyboard.isHold('k'))
			{
				BulletData b;
				b.m_life = 2.f;
				b.m_pos = glm::vec4(m_pos, 0.3f);
				b.m_vel = glm::vec4(m_dir, 1.f) * 63.3f;
				b.m_type = 0;
				b.m_map_index = sScene::Order().calcMapIndex(b.m_pos);
				m_bullet.push_back(b);
			}
			if (!m_bullet.empty())
			{
				sBulletSystem::Order().shoot(m_bullet);
			}
		}
	}
};

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\999_game\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(220.f, 20.f, 0.f);
	camera->getData().m_target = glm::vec3(220.f, 0.f, 51.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::App app;
	app.setup(gpu);

	auto context = app.m_context;

	cModelPipeline model_pipeline;
	cModel model;
	std::shared_ptr<Model> render;
	Player m_player;
	m_player.m_pos.x = 223.f;
	m_player.m_pos.z = 183.f;
	{
		sScene::Order().setup(context);

		model.load(context, "..\\..\\resource\\tiny.x");
		model_pipeline.setup(context);
		render = model_pipeline.createRender(context, model.getResource());
		{
			PlayMotionDescriptor desc;
			desc.m_data = model.getResource()->getAnimation().m_motion[0];
			desc.m_play_no = 0;
			desc.m_start_time = 0.f;
			render->m_animation->getPlayList().play(desc);

			auto& transform = render->m_animation->getModelTransform();
			transform.m_local_scale = glm::vec3(0.02f);
			transform.m_local_rotate = glm::quat(1.f, 0.f, 0.f, 0.f);
			transform.m_local_translate = glm::vec3(0.f, 0.f, 0.f);
		}

 		sBoid::Order().setup(context);
		sBulletSystem::Order().setup(context);
 		sCollisionSystem::Order().setup(context);

		sScene::Order().getVoxel().createPipeline<MapVoxelize>(context);
	}

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			
			{
				m_player.execute(context);
				render->m_animation->getModelTransform().m_global = glm::translate(m_player.m_pos) * glm::toMat4(glm::quat(glm::vec3(0.f, 0.f, 1.f), m_player.m_dir));
			}

			SynchronizedPoint motion_worker_syncronized_point(1);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render->m_animation->update();
					motion_worker_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}

			SynchronizedPoint render_syncronized_point(6);
			std::vector<vk::CommandBuffer> render_cmds(9);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[0] = sScene::Order().draw1(context);
					render_cmds[1] = sScene::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[2] = sScene::Order().getVoxel().make(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[3] = model_pipeline.draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[4] = sBoid::Order().execute(context);
					render_cmds[5] = sBoid::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[6] = sBulletSystem::Order().execute(context);
					render_cmds[7] = sBulletSystem::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[8] = sCollisionSystem::Order().execute(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
#if 0 // voxelize check
			SynchronizedPoint render_syncronized_point(1);
			std::vector<vk::CommandBuffer> render_cmds(3);
			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
 					render_cmds[0] = sScene::Order().draw1(executer);
					render_cmds[1] = voxelize_pipeline.make(executer);
					render_cmds[2] = voxelize_pipeline.draw(executer);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}
#endif
			render_syncronized_point.wait();
			app.submit(std::move(render_cmds));

			motion_worker_syncronized_point.wait();

		}
		app.postUpdate();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

