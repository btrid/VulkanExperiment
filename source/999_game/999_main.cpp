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
#include <applib/DrawHelper.h>
#include <btrlib/Context.h>
#include <btrlib/VoxelPipeline.h>

#include <999_game/sBulletSystem.h>
#include <999_game/sBoid.h>
#include <999_game/sCollisionSystem.h>
#include <999_game/sLightSystem.h>
#include <999_game/sScene.h>
#include <999_game/sMap.h>
#include <999_game/sMap_RayMarch.h>
#include <999_game/VoxelizeMap.h>
#include <999_game/VoxelizeBullet.h>

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

struct ModelGIPipelineComponent : public ModelDrawPipelineComponent
{
	ModelGIPipelineComponent(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderPassModule>& render_pass, const std::shared_ptr<ShaderModule>& shader)
	{
		auto& device = context->m_device;
		m_render_pass = render_pass;
		m_shader = shader;

		// Create descriptor set
		{
			m_model_descriptor = std::make_shared<ModelDescriptorModule>(context);
		}


		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_model_descriptor->getLayout(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				sMap::Order().getVoxel().getDescriptorSetLayout(VoxelPipeline::DESCRIPTOR_SET_LAYOUT_VOXEL),
				sLightSystem::Order().getDescriptorSetLayout(sLightSystem::DESCRIPTOR_SET_LAYOUT_LIGHT),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);

			m_pipeline_layout = device->createPipelineLayoutUnique(pipeline_layout_info);

		}

		// pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleList);

			vk::Extent3D size;
			size.setWidth(context->m_window->getClientSize().x);
			size.setHeight(context->m_window->getClientSize().y);
			size.setDepth(1);
			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height));
			vk::PipelineViewportStateCreateInfo viewport_info;
			viewport_info.setViewportCount(1);
			viewport_info.setPViewports(&viewport);
			viewport_info.setScissorCount(1);
			viewport_info.setPScissors(&scissor);

			// ラスタライズ
			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eBack);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setLineWidth(1.f);
			// サンプリング
			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			// デプスステンシル
			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			// ブレンド
			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			auto vertex_input_binding = cModel::GetVertexInputBinding();
			auto vertex_input_attribute = cModel::GetVertexInputAttribute();
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
			vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
			vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
			vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount((uint32_t)shader->getShaderStageInfo().size())
				.setPStages(shader->getShaderStageInfo().data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout.get())
				.setRenderPass(render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}

	std::shared_ptr<ModelRender> createRender(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<Model>& model) override
	{
		auto& device = context->m_device;
		auto render = std::make_shared<ModelRender>();

		render->m_descriptor_set_model = m_model_descriptor->allocateDescriptorSet(context);
		m_model_descriptor->updateMaterial(context, render->m_descriptor_set_model.get(), model->m_material);
		m_model_descriptor->updateAnimation(context, render->m_descriptor_set_model.get(), model->m_animation);

		// recode command
		{
			vk::CommandBufferAllocateInfo cmd_buffer_info;
			cmd_buffer_info.commandBufferCount = context->m_window->getSwapchain().getBackbufferNum();
			cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
			cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
			render->m_draw_cmd = context->m_device->allocateCommandBuffersUnique(cmd_buffer_info);

			for (size_t i = 0; i < render->m_draw_cmd.size(); i++)
			{
				auto& cmd = render->m_draw_cmd[i].get();
				vk::CommandBufferBeginInfo begin_info;
				begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
				vk::CommandBufferInheritanceInfo inheritance_info;
				inheritance_info.setFramebuffer(getRenderPassModule()->getFramebuffer(i));
				inheritance_info.setRenderPass(getRenderPassModule()->getRenderPass());
				begin_info.pInheritanceInfo = &inheritance_info;

				cmd.begin(begin_info);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, render->m_descriptor_set_model.get(), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 2, sMap::Order().getVoxel().getDescriptorSet(VoxelPipeline::DESCRIPTOR_SET_VOXEL), {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 3, sLightSystem::Order().getDescriptorSet(sLightSystem::DESCRIPTOR_SET_LIGHT), {});
				cmd.bindVertexBuffers(0, { model->m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().buffer }, { model->m_model_resource->m_mesh_resource.m_vertex_buffer_ex.getBufferInfo().offset });
				cmd.bindIndexBuffer(model->m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().buffer, model->m_model_resource->m_mesh_resource.m_index_buffer_ex.getBufferInfo().offset, model->m_model_resource->m_mesh_resource.mIndexType);
				cmd.drawIndexedIndirect(model->m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().buffer, model->m_model_resource->m_mesh_resource.m_indirect_buffer_ex.getBufferInfo().offset, model->m_model_resource->m_mesh_resource.mIndirectCount, sizeof(cModel::Mesh));

				cmd.end();
			}
		}

		return render;
	}
	virtual const std::shared_ptr<RenderPassModule>& getRenderPassModule()const override { return m_render_pass; }

private:
	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_pipeline_layout;
	std::shared_ptr<ModelDescriptorModule> m_model_descriptor;
	std::shared_ptr<RenderPassModule> m_render_pass;
	std::shared_ptr<ShaderModule> m_shader;
};

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\999_game\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(220.f, 40.f, 30.f);
	camera->getData().m_target = glm::vec3(220.f, 10.f, 191.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 50000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(1200, 800);
	app::App app(app_desc);

	auto context = app.m_context;

	cModelPipeline model_pipeline;
	cModel model;
	std::shared_ptr<Model> render;
	Player m_player;
	m_player.m_pos.x = 223.f;
	m_player.m_pos.z = 183.f;
	{

		model.load(context, "..\\..\\resource\\tiny.x");

		sScene::Order().setup(context);
		sBoid::Order().setup(context);
		sBulletSystem::Order().setup(context);
		sCollisionSystem::Order().setup(context);
		sLightSystem::Order().setup(context);
		sMap::Order().setup(context);
		sMap::Order().getVoxel().createPipeline<VoxelizeMap>(context);

		{
			auto render_pass = app::g_app_instance->m_window->getRenderBackbufferPass();
			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			std::vector<ShaderDescriptor> shader_desc =
			{
				{ path + "ModelRender.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ path + "ModelRender.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			auto shader = std::make_shared<ShaderModule>(context, shader_desc);
			auto pipeline = std::make_shared<ModelGIPipelineComponent>(context, render_pass, shader);
			model_pipeline.setup(context, pipeline);
		}
		render = model_pipeline.createRender(context, model.getResource());
		{
			PlayMotionDescriptor desc;
			desc.m_data = model.getResource()->getAnimation().m_motion[0];
			desc.m_play_no = 0;
			desc.m_start_time = 0.f;
			render->m_animation->getPlayList().play(desc);

			auto& transform = render->m_animation->getTransform();
			transform.m_local_scale = glm::vec3(0.02f);
			transform.m_local_rotate = glm::quat(1.f, 0.f, 0.f, 0.f);
			transform.m_local_translate = glm::vec3(0.f, 0.f, 0.f);
		}

	}

	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			{
				m_player.execute(context);
				render->m_animation->getTransform().m_global = glm::translate(m_player.m_pos) * glm::toMat4(glm::quat(glm::vec3(0.f, 0.f, 1.f), m_player.m_dir));
			}

			SynchronizedPoint motion_worker_syncronized_point(1);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render->m_animation->animationUpdate();
					motion_worker_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}

			SynchronizedPoint render_syncronized_point(7);
			std::vector<vk::CommandBuffer> render_cmds(11);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[0] = sScene::Order().execute(context);
					render_cmds[6] = sMap::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[7] = model_pipeline.draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[1] = sBoid::Order().execute(context);
					render_cmds[8] = sBoid::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[2] = sBulletSystem::Order().execute(context);
					render_cmds[9] = sBulletSystem::Order().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[3] = sCollisionSystem::Order().execute(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					render_cmds[4] = sLightSystem::Order().execute(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
//					render_cmds[5] = sMap::Order().getVoxel().make(context);
//					render_cmds[10] = sMap::Order().getVoxel().draw(context);
					render_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_syncronized_point.wait();
			app.submit(std::move(render_cmds));
			motion_worker_syncronized_point.wait();


		}
		app.postUpdate();
		printf("%6.4fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

