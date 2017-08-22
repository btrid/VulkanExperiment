#include <applib/App.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cDebug.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "vulkan-1.lib")

namespace app
{

App::App()
{
	sWindow& w = sWindow::Order();
	vk::Instance instance = sGlobal::Order().getVKInstance();

#if _DEBUG
	static cDebug debug(instance);
#endif

	cWindow::CreateInfo windowInfo;
	windowInfo.surface_format_request = vk::SurfaceFormatKHR{ vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };
	windowInfo.gpu = sGlobal::Order().getGPU(0);
	windowInfo.size = vk::Extent2D(640, 480);
	windowInfo.window_name = L"Vulkan Test";
	windowInfo.class_name = L"VulkanMainWindow";

	m_window.setup(windowInfo);


	cGPU& gpu = sGlobal::Order().getGPU(0);
	cDevice device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];

	{
		m_backbuffer_view.resize(m_window.getSwapchain().getSwapchainNum());
		for (uint32_t i = 0; i < m_window.getSwapchain().getSwapchainNum(); i++)
		{
			auto subresourceRange = vk::ImageSubresourceRange()
				.setAspectMask(vk::ImageAspectFlagBits::eColor)
				.setBaseArrayLayer(0)
				.setLayerCount(1)
				.setBaseMipLevel(0)
				.setLevelCount(1);

			vk::ImageViewCreateInfo backbuffer_view_info;
			backbuffer_view_info.setImage(m_window.getSwapchain().m_backbuffer_image[i]);
			backbuffer_view_info.setFormat(m_window.getSwapchain().m_surface_format.format);
			backbuffer_view_info.setViewType(vk::ImageViewType::e2D);
			backbuffer_view_info.setComponents(vk::ComponentMapping{
				vk::ComponentSwizzle::eR,
				vk::ComponentSwizzle::eG,
				vk::ComponentSwizzle::eB,
				vk::ComponentSwizzle::eA,
			});
			backbuffer_view_info.setSubresourceRange(subresourceRange);
			m_backbuffer_view[i] = device->createImageView(backbuffer_view_info);

		}
	}

	// レンダーパス
	{
		// sub pass
		std::vector<vk::AttachmentReference> colorRef =
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
		subpass.setColorAttachmentCount((uint32_t)colorRef.size());
		subpass.setPColorAttachments(colorRef.data());
		subpass.setPDepthStencilAttachment(&depth_ref);
		// render pass
		std::vector<vk::AttachmentDescription> attachDescription = {
			// color1
			vk::AttachmentDescription()
			.setFormat(m_window.getSwapchain().m_surface_format.format)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			vk::AttachmentDescription()
			.setFormat(vk::Format::eD32Sfloat)
			.setSamples(vk::SampleCountFlagBits::e1)
			.setLoadOp(vk::AttachmentLoadOp::eClear)
			.setStoreOp(vk::AttachmentStoreOp::eStore)
			.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
			.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),

		};
		vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
			.setAttachmentCount(attachDescription.size())
			.setPAttachments(attachDescription.data())
			.setSubpassCount(1)
			.setPSubpasses(&subpass);

		m_render_pass = device->createRenderPass(renderpass_info);
	}

	{
		// イメージ生成
		vk::ImageCreateInfo depth_info;
		depth_info.format = vk::Format::eD32Sfloat;
		depth_info.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
		depth_info.arrayLayers = 1;
		depth_info.mipLevels = 1;
		depth_info.extent.width = m_window.getClientSize().x;
		depth_info.extent.height = m_window.getClientSize().y;
		depth_info.extent.depth = 1;
		depth_info.imageType = vk::ImageType::e2D;
		depth_info.initialLayout = vk::ImageLayout::eUndefined;
		depth_info.queueFamilyIndexCount = device.getQueueFamilyIndex().size();
		depth_info.pQueueFamilyIndices = device.getQueueFamilyIndex().data();
		m_depth_image = device->createImage(depth_info);

		// メモリ確保
		auto memory_request = device->getImageMemoryRequirements(m_depth_image);
		uint32_t memory_index = cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::MemoryAllocateInfo memory_info;
		memory_info.allocationSize = memory_request.size;
		memory_info.memoryTypeIndex = memory_index;
		m_depth_memory = device->allocateMemory(memory_info);

		device->bindImageMemory(m_depth_image, m_depth_memory, 0);

		vk::ImageViewCreateInfo depth_view_info;
		depth_view_info.format = depth_info.format;
		depth_view_info.image = m_depth_image;
		depth_view_info.viewType = vk::ImageViewType::e2D;
		depth_view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
		depth_view_info.subresourceRange.baseArrayLayer = 0;
		depth_view_info.subresourceRange.baseMipLevel = 0;
		depth_view_info.subresourceRange.layerCount = 1;
		depth_view_info.subresourceRange.levelCount = 1;
		m_depth_view = device->createImageView(depth_view_info);

	}
	m_framebuffer.resize(m_backbuffer_view.size());
	{
		std::vector<vk::ImageView> view(2);

		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass);
		framebuffer_info.setAttachmentCount((uint32_t)view.size());
		framebuffer_info.setPAttachments(view.data());
		framebuffer_info.setWidth(m_window.getClientSize().x);
		framebuffer_info.setHeight(m_window.getClientSize().y);
		framebuffer_info.setLayers(1);

		for (size_t i = 0; i < m_backbuffer_view.size(); i++) {
			view[0] = m_backbuffer_view[i];
			view[1] = m_depth_view;
			m_framebuffer[i] = device->createFramebuffer(framebuffer_info);
		}
	}

}


glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size)
{
	glm::uvec3 ret = (num + local_size - glm::uvec3(1)) / local_size;
	return ret;
}

}
