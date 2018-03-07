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
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct ClearPipeline
{
	ClearPipeline(btr::Context* context, const RenderTarget& render_target)
	{
		vk::CommandBufferAllocateInfo cmd_buffer_info;
		cmd_buffer_info.commandPool = context->m_cmd_pool->getCmdPool(cCmdPool::CMD_POOL_TYPE_COMPILED, 0);
		cmd_buffer_info.commandBufferCount = 1;
		cmd_buffer_info.level = vk::CommandBufferLevel::ePrimary;
		m_cmd = std::move(context->m_device->allocateCommandBuffersUnique(cmd_buffer_info)[0]);
		context->m_device.DebugMarkerSetObjectName(m_cmd.get(), "clear cmd");

		auto cmd = m_cmd.get();
		vk::CommandBufferBeginInfo begin_info;
		begin_info.setFlags(vk::CommandBufferUsageFlagBits::eSimultaneousUse);
		vk::CommandBufferInheritanceInfo inheritance_info;
		begin_info.setPInheritanceInfo(&inheritance_info);
		cmd.begin(begin_info);

		{

			vk::ImageMemoryBarrier present_to_clear;
			//present_to_clear.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			present_to_clear.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			//present_to_clear.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
			present_to_clear.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
			present_to_clear.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			present_to_clear.setImage(render_target.m_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eAllCommands,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				nullptr, nullptr, present_to_clear);

			vk::ClearColorValue clear_color;
			clear_color.setFloat32(std::array<float, 4>{0.f, 0.f, 1.f, 0.f});
			cmd.clearColorImage(render_target.m_image, vk::ImageLayout::eTransferDstOptimal, clear_color, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });

		}

		{
			vk::ImageMemoryBarrier clear_to_render;
			clear_to_render.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			clear_to_render.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			clear_to_render.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
			clear_to_render.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			clear_to_render.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			clear_to_render.setImage(render_target.m_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::DependencyFlags(),
				nullptr, nullptr, clear_to_render);
		}

		{
			vk::ImageMemoryBarrier render_to_clear_depth;
			render_to_clear_depth.setSrcAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
			render_to_clear_depth.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
			render_to_clear_depth.setOldLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
			render_to_clear_depth.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
			render_to_clear_depth.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
			render_to_clear_depth.setImage(render_target.m_depth_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eLateFragmentTests,
				vk::PipelineStageFlagBits::eTransfer,
				vk::DependencyFlags(),
				nullptr, nullptr, render_to_clear_depth);

			vk::ClearDepthStencilValue clear_depth;
			clear_depth.setDepth(0.f);
			cmd.clearDepthStencilImage(render_target.m_depth_image, vk::ImageLayout::eTransferDstOptimal, clear_depth, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
		}

		{
			vk::ImageMemoryBarrier clear_to_render_depth;
			clear_to_render_depth.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			clear_to_render_depth.setDstAccessMask(vk::AccessFlagBits::eDepthStencilAttachmentWrite);
			clear_to_render_depth.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
			clear_to_render_depth.setNewLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);
			clear_to_render_depth.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
			clear_to_render_depth.setImage(render_target.m_depth_image);
			cmd.pipelineBarrier(
				vk::PipelineStageFlagBits::eTransfer,
				vk::PipelineStageFlagBits::eLateFragmentTests,
				vk::DependencyFlags(),
				nullptr, nullptr, clear_to_render_depth);

		}

		cmd.end();
	}

	vk::CommandBuffer execute() {
		return m_cmd.get();
	}

	vk::UniqueCommandBuffer m_cmd;
};

int main()
{
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(640, 480);
	app::App app(app_desc);

	auto context = app.m_context;

	app.setup();

	auto render_target = app.m_window->getRenderTarget();
	ClearPipeline clear_render_target(context.get(), render_target);
	auto present_cmds = app.m_window->present(context, render_target);
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			std::vector<vk::CommandBuffer> cmds =
			{
				clear_render_target.execute(),
				present_cmds.cmds[context->getGPUFrame()].get(),
			};
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

