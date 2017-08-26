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


	auto& gpu = sGlobal::Order().getGPU(0);
	auto& device = gpu.getDevice();

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

	m_framebuffer.resize(m_window.getSwapchain().m_backbuffer.size());
	{
		std::vector<vk::ImageView> view(2);

		vk::FramebufferCreateInfo framebuffer_info;
		framebuffer_info.setRenderPass(m_render_pass);
		framebuffer_info.setAttachmentCount((uint32_t)view.size());
		framebuffer_info.setPAttachments(view.data());
		framebuffer_info.setWidth(m_window.getClientSize().x);
		framebuffer_info.setHeight(m_window.getClientSize().y);
		framebuffer_info.setLayers(1);

		for (size_t i = 0; i < m_window.getSwapchain().m_backbuffer.size(); i++) {
			view[0] = m_window.getSwapchain().m_backbuffer[i].m_view;
			view[1] = m_window.getSwapchain().m_depth.m_view;
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
