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
#include <btrlib/Loader.h>
#include <applib/Geometry.h>
// #define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
// #include <imgui/imgui.h>

#include <006_voxel_based_GI/VoxelPipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
//#pragma comment(lib, "imgui.lib")


int main()
{
	vec3 albedo = vec3(1.4f, 0.2f, 0.7f);
	uint packd_albedo = uint(albedo.r * 64) << 23 | uint(albedo.g * 64) << 14 | uint(albedo.b * 64) << 5 | 1;
	float packd_albedo_r = ((packd_albedo >> 23)&((1 << 9) - 1)) / 64.f;
	float packd_albedo_g = ((packd_albedo >> 14)&((1 << 9) - 1)) / 64.f;
	float packd_albedo_b = ((packd_albedo >> 5)&((1 << 9) - 1)) / 64.f;
	uint count = (packd_albedo)&((1 << 5) - 1);

	btr::setResourceAppPath("..\\..\\resource\\006_voxel_based_GI\\");
	auto* camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1200.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	vk::Queue queue = device->getQueue(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics), 0);

	std::shared_ptr<btr::Loader> loader = std::make_shared<btr::Loader>();
	auto executer = std::make_shared<btr::Executer>();
	{
		loader->m_gpu = gpu;
		loader->m_device = device;

		vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
		vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;
		//	device_memory = host_memory; // debug
		loader->m_vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 100);
		loader->m_uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 20);
		loader->m_storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 200);
		loader->m_staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferSrc, host_memory, 1000 * 1000 * 100);

		std::vector<vk::DescriptorPoolSize> pool_size(4);
		pool_size[0].setType(vk::DescriptorType::eUniformBuffer);
		pool_size[0].setDescriptorCount(10);
		pool_size[1].setType(vk::DescriptorType::eStorageBuffer);
		pool_size[1].setDescriptorCount(20);
		pool_size[2].setType(vk::DescriptorType::eCombinedImageSampler);
		pool_size[2].setDescriptorCount(10);
		pool_size[3].setType(vk::DescriptorType::eStorageImage);
		pool_size[3].setDescriptorCount(10);

		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setPoolSizeCount((uint32_t)pool_size.size());
		pool_info.setPPoolSizes(pool_size.data());
		pool_info.setMaxSets(20);
		loader->m_descriptor_pool = device->createDescriptorPoolUnique(pool_info);

		vk::PipelineCacheCreateInfo cacheInfo = vk::PipelineCacheCreateInfo();
		loader->m_cache = device->createPipelineCacheUnique(cacheInfo);
	}
	{
		executer->m_gpu = gpu;
		executer->m_device = device;
		executer->m_vertex_memory = loader->m_vertex_memory;
		executer->m_uniform_memory = loader->m_uniform_memory;
		executer->m_storage_memory = loader->m_storage_memory;
		executer->m_staging_memory = loader->m_staging_memory;
	}

	app::App app;
	app.setup(loader);

	executer->m_window = app.m_window;
	loader->m_window = app.m_window;
	loader->m_cmd_pool = app.m_cmd_pool;
	executer->m_cmd_pool = app.m_cmd_pool;
	VoxelPipeline voxelize;
	{
		auto setup_cmd = loader->m_cmd_pool->allocCmdOnetime(0);
		sCameraManager::Order().setup(loader);
		DrawHelper::Order().setup(loader);
		voxelize.setup(loader);
		{
			std::vector<glm::vec3> v;
			std::vector<glm::uvec3> i;
			std::tie(v, i) = Geometry::MakeSphere();

			VoxelizeModel model;
			model.m_mesh.resize(1);
			model.m_mesh[0].vertex.resize(v.size());
			for (size_t i = 0; i < model.m_mesh[0].vertex.size(); i++)
			{
				model.m_mesh[0].vertex[i].pos = v[i]*200.f;
			}
			model.m_mesh[0].index = i;
			model.m_mesh[0].m_material_index = 0;

			model.m_material[0].albedo = glm::vec4(1.f, 0.f, 0.f, 1.f);
			model.m_material[0].emission = glm::vec4(0.f, 0.f, 1.f, 1.f);

			voxelize.addModel(executer, setup_cmd, model);
		}

		setup_cmd.end();
		std::vector<vk::CommandBuffer> cmds = {
			setup_cmd,
		};

		vk::PipelineStageFlags waitPipeline = vk::PipelineStageFlagBits::eAllGraphics;
		std::vector<vk::SubmitInfo> submitInfo =
		{
			vk::SubmitInfo()
			.setCommandBufferCount((uint32_t)cmds.size())
			.setPCommandBuffers(cmds.data())
		};
		queue.submit(submitInfo, vk::Fence());
		queue.waitIdle();

	}


	while (true)
	{
		cStopWatch time;

		uint32_t backbuffer_index = app.m_window->getSwapchain().swap();

		sDebug::Order().waitFence(device.getHandle(), app.m_window->getFence(backbuffer_index));
		device->resetFences({ app.m_window->getFence(backbuffer_index)});
		app.m_cmd_pool->resetPool(executer);

		{
			{
				auto* m_camera = cCamera::sCamera::Order().getCameraList()[0];
				m_camera->control(app.m_window->getInput(), 0.016f);

				DrawCommand dcmd;
				dcmd.world = glm::scale(vec3(200.f));
				DrawHelper::Order().drawOrder(DrawHelper::SPHERE, dcmd);
			}

			SynchronizedPoint motion_worker_syncronized_point(1);
			SynchronizedPoint render_syncronized_point(2);
			SynchronizedPoint loader_syncronized_point(1);
			std::vector<vk::CommandBuffer> render_cmds(5);

			{
				cThreadJob job;
				job.mJob.emplace_back(
					[&]()
				{
					render_cmds[1] = sCameraManager::Order().draw(executer);
					render_cmds[2] = DrawHelper::Order().draw(executer);
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
					render_cmds[3] = voxelize.draw(executer);
					render_syncronized_point.arrive();
				}
				);
				sGlobal::Order().getThreadPool().enque(job);
			}

			// draw
			render_cmds.front() = app.m_window->getSwapchain().m_cmd_present_to_render[backbuffer_index];
			render_cmds.back() = app.m_window->getSwapchain().m_cmd_render_to_present[backbuffer_index];

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

			render_syncronized_point.wait();
			loader_syncronized_point.wait();

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

