#include <btrlib/cWindow.h>
#include <btrlib/Context.h>

namespace
{

std::vector<uint32_t> getSupportSurfaceQueue(vk::PhysicalDevice gpu, vk::SurfaceKHR surface)
{
	auto queueProp = gpu.getQueueFamilyProperties();
	std::vector<uint32_t> queueIndexList;
	queueIndexList.reserve(queueProp.size());
	for (size_t i = 0; i < queueProp.size(); i++)
	{
		if (gpu.getSurfaceSupportKHR((uint32_t)i, surface)) {
			queueIndexList.emplace_back((uint32_t)i);
		}
	}

	assert(!queueIndexList.empty());
	return queueIndexList;
}

}

void cWindow::Swapchain::setup(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor, vk::SurfaceKHR surface)
{
	m_context = context;
	std::vector<vk::PresentModeKHR> presentModeList = context->m_gpu->getSurfacePresentModesKHR(surface);

	m_surface_format.format = vk::Format::eUndefined;
	std::vector<vk::SurfaceFormatKHR> support_format = context->m_gpu->getSurfaceFormatsKHR(surface);

	auto request = { 
		descriptor.surface_format_request,
	};
	for (auto& r : request)
	{
		auto it = std::find(support_format.begin(), support_format.end(), r);
		if (it != support_format.end()) {
			m_surface_format = r;
			break;
		}
	}

	// サーフェスに使用できるフォーマットが見つからなかった
	assert(m_surface_format.format != vk::Format::eUndefined);

	auto support_surface = getSupportSurfaceQueue(context->m_gpu.getHandle(), surface);
	auto device = context->m_gpu.getDevice();

	auto old_swap_chain = m_swapchain_handle.get();

	vk::SurfaceCapabilitiesKHR capability = context->m_gpu->getSurfaceCapabilitiesKHR(surface);
	vk::SwapchainCreateInfoKHR swapchain_info;
	swapchain_info.setSurface(surface);
	swapchain_info.setMinImageCount(3);
	swapchain_info.setImageFormat(m_surface_format.format);
	swapchain_info.setImageColorSpace(m_surface_format.colorSpace);
	swapchain_info.setImageExtent(capability.currentExtent);
	swapchain_info.setPreTransform(capability.currentTransform);
	swapchain_info.setImageArrayLayers(1);
	swapchain_info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst);
	swapchain_info.setImageSharingMode(vk::SharingMode::eExclusive);
	swapchain_info.setQueueFamilyIndexCount((uint32_t)support_surface.size());
	swapchain_info.setPQueueFamilyIndices(support_surface.data());
	swapchain_info.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
	swapchain_info.setPresentMode(vk::PresentModeKHR::eMailbox);
	swapchain_info.setClipped(true);
	swapchain_info.setOldSwapchain(m_swapchain_handle.get());
	m_swapchain_handle = device->createSwapchainKHRUnique(swapchain_info);

	m_backbuffer_image = device->getSwapchainImagesKHR(m_swapchain_handle.get());
	m_size = capability.currentExtent;
	


}

uint32_t cWindow::Swapchain::swap()
{
	m_context->m_device->acquireNextImageKHR(m_swapchain_handle.get(), 1000, m_swapbuffer_semaphore.get(), vk::Fence(), &m_backbuffer_index);
	return m_backbuffer_index;
}

cWindow::cWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor) 
	: m_private(std::make_shared<Private>())
	, m_surface()
{
	m_descriptor = descriptor;
	WNDCLASSEXW wcex = {};

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = GetModuleHandle(nullptr);
	wcex.lpszClassName = m_descriptor.class_name.c_str();
	if (!RegisterClassExW(&wcex)) {
		assert(false);
		return;
	}

	DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	DWORD dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	RECT rect{ 0, 0, (LONG)m_descriptor.size.width, (LONG)m_descriptor.size.height };
	AdjustWindowRectEx(&rect, dwStyle, false, dwExStyle);
	m_private->m_window = CreateWindowExW(dwExStyle, m_descriptor.class_name.c_str(), m_descriptor.window_name.c_str(), dwStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

	assert(m_private->m_window);

	ShowWindow(m_private->m_window, 1);
	UpdateWindow(m_private->m_window);

	// vulkan setup
	vk::Win32SurfaceCreateInfoKHR surfaceInfo = vk::Win32SurfaceCreateInfoKHR()
		.setHinstance(GetModuleHandle(nullptr))
		.setHwnd(m_private->m_window);
	m_surface = sGlobal::Order().getVKInstance().createWin32SurfaceKHRUnique(surfaceInfo);

	m_swapchain.setup(context, descriptor, m_surface.get());

	vk::SemaphoreCreateInfo semaphoreInfo = vk::SemaphoreCreateInfo();
	m_swapchain.m_swapbuffer_semaphore = context->m_gpu.getDevice()->createSemaphoreUnique(semaphoreInfo);
	m_swapchain.m_submit_semaphore = context->m_gpu.getDevice()->createSemaphoreUnique(semaphoreInfo);

}

