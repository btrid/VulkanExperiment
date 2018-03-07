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
#include <applib/cModelInstancingPipeline.h>
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

struct ModelGIPipelineComponent
{
	ModelGIPipelineComponent(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
	{
		auto& device = context->m_device;

		// レンダーパス
		{
			// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};
			vk::AttachmentReference depth_ref;
			depth_ref.setAttachment(1);
			depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);
			subpass.setPDepthStencilAttachment(&depth_ref);

			vk::AttachmentDescription attach_description[] = {
				// color1
				vk::AttachmentDescription()
				.setFormat(render_target->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				vk::AttachmentDescription()
				.setFormat(render_target->m_depth_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(array_length(attach_description))
				.setPAttachments(attach_description)
				.setSubpassCount(1)
				.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}

		{
			vk::ImageView view[2];
			view[0] = render_target->m_view;
			view[1] = render_target->m_depth_view;

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(render_target->m_info.extent.width);
			framebuffer_info.setHeight(render_target->m_info.extent.height);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
		}

		{
			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			std::string shader_desc[] =
			{
				{ path + "ModelRender.vert.spv"},
				{ path + "ModelRender.frag.spv"},
			};
			for (size_t i = 0; i < array_length(shader_desc); i++) {
				m_shader[i] = std::move(loadShaderUnique(device.getHandle(), shader_desc[i]));
			}

		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				sModelRenderDescriptor::Order().getLayout(),
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

			vk::PipelineShaderStageCreateInfo stage_info[] = {
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[0].get())
				.setStage(vk::ShaderStageFlagBits::eVertex)
				.setPName("main"),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[1].get())
				.setStage(vk::ShaderStageFlagBits::eFragment)
				.setPName("main"),
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(stage_info))
				.setPStages(stage_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout.get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			m_pipeline = std::move(device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info)[0]);
		}

	}

	vk::UniqueCommandBuffer createCmd(const std::shared_ptr<btr::Context>& context, const Drawable* drawable, const DescriptorSet<ModelRenderDescriptor::Set>& descriptor_set)
	{
		auto& device = context->m_device;

		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.level = vk::CommandBufferLevel::eSecondary;
		auto cmd = std::move(context->m_device->allocateCommandBuffersUnique(cmd_buffer_info)[0]);
		// recode command
		{
			vk::CommandBufferBeginInfo begin_info;
			begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse | vk::CommandBufferUsageFlagBits::eRenderPassContinue);
			vk::CommandBufferInheritanceInfo inheritance_info;
			inheritance_info.setFramebuffer(m_framebuffer.get());
			inheritance_info.setRenderPass(m_render_pass.get());
			begin_info.pInheritanceInfo = &inheritance_info;
			cmd->begin(begin_info);

			cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, descriptor_set.m_handle.get(), {});
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 2, sMap::Order().getVoxel().getDescriptorSet(VoxelPipeline::DESCRIPTOR_SET_VOXEL), {});
			cmd->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 3, sLightSystem::Order().getDescriptorSet(sLightSystem::DESCRIPTOR_SET_LIGHT), {});

			drawable->draw(cmd.get());
			cmd->end();
		}

		return cmd;
	}

private:
	vk::UniqueShaderModule m_shader[2];
	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;
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
		sBoid::Order().setup(context);
		sBulletSystem::Order().setup(context);
		sCollisionSystem::Order().setup(context);
		sLightSystem::Order().setup(context);
		sMap::Order().setup(context);
		sMap::Order().getVoxel().createPipeline<VoxelizeMap>(context);

	}
	sModelRenderDescriptor::Create(context);
	sModelAnimateDescriptor::Create(context);

	AppModelInstancingRenderer renderer(context, app.m_window->getRenderTarget());
	AppModelInstancingAnimationPipeline animater(context);

	std::shared_ptr<AppModel> player_model = std::make_shared<AppModel>(context, model.getResource(), 1);
	DescriptorSet<ModelRenderDescriptor::Set> render_descriptor = createRenderDescriptorSet(player_model);
	DescriptorSet<ModelAnimateDescriptor::Set> animate_descriptor = createAnimateDescriptorSet(player_model);

	auto drawCmd = renderer.createCmd(context, &player_model->m_render, render_descriptor);
	auto animeCmd = animater.createCmd(context, animate_descriptor);

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
//					render->m_animation->animationUpdate();
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
					{
						std::vector<vk::CommandBuffer> cmds(1);
						cmds[0] = animeCmd.get();
						render_cmds[7] = animater.dispach(context, cmds);
					}
					{
						std::vector<vk::CommandBuffer> cmds(1);
						cmds[0] = drawCmd.get();
						render_cmds[8] = renderer.draw(context, cmds);
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
					render_cmds[1] = sBoid::Order().execute(context);
					render_cmds[9] = sBoid::Order().draw(context);
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
					render_cmds[10] = sBulletSystem::Order().draw(context);
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
		printf("%6.4fs\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

