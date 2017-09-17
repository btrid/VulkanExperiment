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
#include <btrlib/BufferMemory.h>

#include <applib/App.h>
#include <applib/cModelPipeline.h>
#include <applib/cModelRender.h>
#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>
#include <btrlib/Loader.h>

#include <applib/sParticlePipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\003_particle\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(20.f, 10.f, 30.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::App app;
	app.setup(gpu);

	auto loader = app.m_loader;
	auto executer = app.m_executer;

	cModelPipeline model_pipeline;
	std::shared_ptr<cModelRender> model_render = std::make_shared<cModelRender>();
	cModel model;

	{
		sParticlePipeline::Order().setup(loader);

		model.load(loader.get(), "..\\..\\resource\\tiny.x");
		model_render->setup(loader, model.getResource());
		model_pipeline.setup(loader);
		model_pipeline.addModel(executer, model_render);
		{
			PlayMotionDescriptor desc;
			desc.m_data = model.getResource()->getAnimation().m_motion[0];
			desc.m_play_no = 0;
			desc.m_start_time = 0.f;
			model_render->getMotionList().play(desc);

			auto& transform = model_render->getModelTransform();
			transform.m_local_scale = glm::vec3(0.002f);
			transform.m_local_rotate = glm::quat(1.f, 0.f, 0.f, 0.f);
			transform.m_local_translate = glm::vec3(0.f, 280.f, 0.f);
		}
	}

	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);
	while (true)
	{
		cStopWatch time;

		uint32_t backbuffer_index = app.m_window->getSwapchain().swap();

		sDebug::Order().waitFence(device.getHandle(), app.m_window->getFence(backbuffer_index));
		device->resetFences({ app.m_window->getFence(backbuffer_index) });
		app.m_cmd_pool->resetPool(executer);

		{


			{
				auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
				m_camera->control(app.m_window->getInput(), 0.016f);

// 				DrawCommand dcmd;
// 				dcmd.world = glm::scale(vec3(5.f));
// 				DrawHelper::Order().drawOrder(DrawHelper::SPHERE, dcmd);
				model_render->getModelTransform().m_global = glm::mat4(1.f);
			}

			SynchronizedPoint render_syncronized_point(2);
			SynchronizedPoint loader_syncronized_point(1);
			std::vector<vk::CommandBuffer> render_cmds(7);
			SynchronizedPoint motion_worker_syncronized_point(1);
			{
				cThreadJob job;
				job.mFinish =
					[&]()
				{
					model_render->work();
					motion_worker_syncronized_point.arrive();
				};
				sGlobal::Order().getThreadPool().enque(job);
			}

			// draw
			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					executer->m_cmd_pool->submit(executer);
					loader_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[3] = model_pipeline.draw(executer);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[4] = sParticlePipeline::Order().execute(executer);
					render_cmds[5] = sParticlePipeline::Order().draw(executer);
					render_cmds[1] = sCameraManager::Order().draw(executer);
					render_cmds[2] = DrawHelper::Order().draw(executer);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			render_cmds.front() = app.m_window->getSwapchain().m_cmd_present_to_render[backbuffer_index];
			render_cmds.back() = app.m_window->getSwapchain().m_cmd_render_to_present[backbuffer_index];

			render_syncronized_point.wait();
			loader_syncronized_point.wait();
			motion_worker_syncronized_point.wait();

			vk::Semaphore swap_wait_semas[] = {
				app.m_window->getSwapchain().m_swapbuffer_semaphore.get(),
			};
			vk::Semaphore submit_wait_semas[] = {
				app.m_window->getSwapchain().m_submit_semaphore.get(),
			};

			vk::PipelineStageFlags wait_pipelines[] = {
				vk::PipelineStageFlagBits::eAllGraphics,
			};
			std::vector<vk::SubmitInfo> submitInfo =
			{
				vk::SubmitInfo()
				.setCommandBufferCount((uint32_t)render_cmds.size())
				.setPCommandBuffers(render_cmds.data())
				.setWaitSemaphoreCount(array_length(swap_wait_semas))
				.setPWaitSemaphores(swap_wait_semas)
				.setPWaitDstStageMask(wait_pipelines)
				.setSignalSemaphoreCount(array_length(submit_wait_semas))
				.setPSignalSemaphores(submit_wait_semas)
			};
			queue.submit(submitInfo, app.m_window->getFence(backbuffer_index));
			queue.waitIdle();
			vk::SwapchainKHR swapchains[] = {
				app.m_window->getSwapchain().m_swapchain_handle.get(),
			};
			uint32_t backbuffer_indexs[] = {
				app.m_window->getSwapchain().m_backbuffer_index,
			};
			vk::PresentInfoKHR present_info = vk::PresentInfoKHR()
				.setWaitSemaphoreCount(array_length(submit_wait_semas))
				.setPWaitSemaphores(submit_wait_semas)
				.setSwapchainCount(array_length(swapchains))
				.setPSwapchains(swapchains)
				.setPImageIndices(backbuffer_indexs);
			queue.presentKHR(present_info);

		}

		app.m_window->update();
		sGlobal::Order().swap();
		sCameraManager::Order().sync();
		sDeleter::Order().swap();
		printf("%6.3fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

