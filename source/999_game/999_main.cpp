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
#include <applib/AppModel/AppModelPipeline.h>
#include <applib/DrawHelper.h>
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>

#include <999_game/sBulletSystem.h>
#include <999_game/sBoid.h>
#include <999_game/sCollisionSystem.h>
#include <999_game/sLightSystem.h>
#include <999_game/sScene.h>
#include <999_game/sMap.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

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


	void execute(std::shared_ptr<btr::Context>& context)
	{
		const cInput& input = context->m_window->getInput();
		auto resolution = context->m_window->getClientSize();
		auto mouse_n1_to_1 = (glm::vec2(input.m_mouse.xy) / glm::vec2(resolution) - 0.5f) * 2.f;
		if (glm::dot(mouse_n1_to_1, mouse_n1_to_1) >= 0.0001f) {
			m_dir = glm::normalize(-glm::vec3(mouse_n1_to_1.x, 0.f, mouse_n1_to_1.y));
		}

		m_inertia *= pow(0.85f, 1.f + sGlobal::Order().getDeltaTime());
		m_inertia.x = glm::abs(m_inertia.x) < 0.001f ? 0.f : m_inertia.x;
		m_inertia.z = glm::abs(m_inertia.z) < 0.001f ? 0.f : m_inertia.z;

		bool is_left = input.m_keyboard.isHold('A')|| input.m_keyboard.isHold(VK_LEFT);
		bool is_right = input.m_keyboard.isHold('D')|| input.m_keyboard.isHold(VK_RIGHT);
		bool is_front = input.m_keyboard.isHold('W')|| input.m_keyboard.isHold(VK_UP);
		bool is_back = input.m_keyboard.isHold('S') || input.m_keyboard.isHold(VK_DOWN);
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
			if (input.m_keyboard.isHold('J'))
			{
				BulletData b;
				b.m_life = 1.f;
				b.m_pos = glm::vec4(m_pos, 0.3f);
				b.m_vel = glm::vec4(m_dir, 1.f) * 33.3f;
				b.m_type = 0;
				b.m_map_index = sScene::Order().calcMapIndex(b.m_pos);
				m_bullet.push_back(b);
			}
			if (input.m_keyboard.isHold('K'))
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
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(220.f, 40.f, 30.f);
	camera->getData().m_target = glm::vec3(220.f, 10.f, 191.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1200;
	camera->getData().m_height = 800;
	camera->getData().m_far = 50000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(1200, 800);
	app::App app(app_desc);

	auto context = app.m_context;

	cModel model;
	Player m_player;
	m_player.m_pos.x = 223.f;
	m_player.m_pos.z = 183.f;
	{

		model.load(context, "..\\..\\resource\\tiny.x");

		sScene::Order().setup(context);
		sBoid::Order().setup(context, app.m_window->getRenderTarget());
		sBulletSystem::Order().setup(context, app.m_window->getRenderTarget());
		sCollisionSystem::Order().setup(context);
		sLightSystem::Order().setup(context);
		sMap::Order().setup(context, app.m_window->getRenderTarget());

	}
	sModelRenderDescriptor::Create(context);
	sModelAnimateDescriptor::Create(context);

	AppModelRenderStage renderer(context, app.m_window->getRenderTarget());
	AppModelAnimationStage animater(context);

	std::shared_ptr<AppModel> player_model = std::make_shared<AppModel>(context, model.getResource(), 1);
	DescriptorSet<AppModelRenderDescriptor::Set> render_descriptor = createRenderDescriptorSet(player_model);
	DescriptorSet<AppModelAnimateDescriptor::Set> animate_descriptor = createAnimateDescriptorSet(player_model);

	auto drawCmd = renderer.createCmd(context, &player_model->m_render, render_descriptor);
	auto animeCmd = animater.createCmd(context, animate_descriptor);

	ClearPipeline clear_render_target(context, app.m_window->getRenderTarget());
	PresentPipeline present_pipeline(context, app.m_window->getRenderTarget(), app.m_window->getSwapchainPtr());

	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			{
				m_player.execute(context);
			}

			SynchronizedPoint motion_worker_syncronized_point(1);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					motion_worker_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			enum 
			{
				cmd_clear_render_target,
				cmd_boid_exeute,
				cmd_bullet_exeute,
				cmd_collision_exeute,
				cmd_light_exeute,
				cmd_scene_exeute,
				cmd_map_draw,
				cmd_player_animate,
				cmd_player_draw,
				cmd_boid_draw,
				cmd_bullet_draw,
				cmd_present,
				cmd_num,
			};
			SynchronizedPoint render_syncronized_point(7);
			std::vector<vk::CommandBuffer> render_cmds(cmd_num);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[cmd_clear_render_target] = clear_render_target.execute();
					render_cmds[cmd_present] = present_pipeline.execute();
					render_cmds[cmd_scene_exeute] = sScene::Order().execute(context);
					render_cmds[cmd_map_draw] = sMap::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					{
						std::vector<vk::CommandBuffer> cmds(1);
						cmds[0] = animeCmd.get();
						render_cmds[cmd_player_animate] = animater.dispach(context, cmds);
					}
					{
						std::vector<vk::CommandBuffer> cmds(1);
						cmds[0] = drawCmd.get();
						render_cmds[cmd_player_draw] = renderer.draw(context, cmds);
					}
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[cmd_boid_exeute] = sBoid::Order().execute(context);
					render_cmds[cmd_boid_draw] = sBoid::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[cmd_bullet_exeute] = sBulletSystem::Order().execute(context);
					render_cmds[cmd_bullet_draw] = sBulletSystem::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[cmd_collision_exeute] = sCollisionSystem::Order().execute(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[cmd_light_exeute] = sLightSystem::Order().execute(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_syncronized_point.wait();
			app.submit(std::move(render_cmds));
			motion_worker_syncronized_point.wait();

		}
		app.postUpdate();
		printf("%6.4fs\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

