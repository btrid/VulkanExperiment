#include <btrlib/cWindow.h>
#include <btrlib/Context.h>

namespace
{
void assert_win(bool expression)
{
	if (expression)
	{
		return;
	}

	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // 既定の言語
		(LPTSTR)&lpMsgBuf,
		0,
		NULL
	);

	// lpMsgBuf 内のすべての挿入シーケンスを処理する。

	// ...

	// 文字列を表示する。

	MessageBox(NULL, (LPCTSTR)lpMsgBuf, L"Error", MB_OK | MB_ICONINFORMATION);

	// バッファを解放する。

	LocalFree(lpMsgBuf);

	assert(expression);
}


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

void Swapchain::setup(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor, vk::SurfaceKHR surface)
{
	m_context = context;
	std::vector<vk::PresentModeKHR> presentModeList = context->m_physical_device.getSurfacePresentModesKHR(surface);

	m_surface_format.format = vk::Format::eUndefined;
	std::vector<vk::SurfaceFormatKHR> support_format = context->m_physical_device.getSurfaceFormatsKHR(surface);

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

	auto support_surface = getSupportSurfaceQueue(context->m_physical_device, surface);
	auto& device = context->m_device;

	auto old_swap_chain = m_swapchain_handle.get();

	auto presentmode_request =
	{
		vk::PresentModeKHR::eMailbox,
		vk::PresentModeKHR::eFifo,
	};
	auto presentmode = context->m_physical_device.getSurfacePresentModesKHR(surface);
	for (auto mode : presentmode_request)
	{
		if (auto it = std::find(presentmode.begin(), presentmode.end(), mode); it != presentmode.end())
		{
			vk::SurfaceCapabilitiesKHR capability = context->m_physical_device.getSurfaceCapabilitiesKHR(surface);
			vk::SwapchainCreateInfoKHR swapchain_info;
			swapchain_info.setSurface(surface);
			swapchain_info.setMinImageCount(sGlobal::BUFFER_COUNT_MAX);
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
			swapchain_info.setPresentMode(mode);
			swapchain_info.setClipped(true);
			swapchain_info.setOldSwapchain(m_swapchain_handle.get());
			m_swapchain_handle = device.createSwapchainKHRUnique(swapchain_info);

			m_backbuffer_image = device.getSwapchainImagesKHR(m_swapchain_handle.get());
			m_size = capability.currentExtent;
			break;
		}
	}

	vk::SemaphoreCreateInfo semaphoreInfo = vk::SemaphoreCreateInfo();
	m_semaphore_imageAvailable = context->m_device.createSemaphoreUnique(semaphoreInfo);
	m_semaphore_renderFinished = context->m_device.createSemaphoreUnique(semaphoreInfo);

}

uint32_t Swapchain::swap()
{
	m_context->m_device.acquireNextImageKHR(m_swapchain_handle.get(), UINT64_MAX, m_semaphore_imageAvailable.get(), vk::Fence(), &m_backbuffer_index);
	return m_backbuffer_index;
}

cWindow::cWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor) 
	: m_private(std::make_shared<Private>())
	, m_surface()
{
	m_descriptor = descriptor;
	m_is_close = false;

	static int classNo;
	wchar_t classname[256] = {};
	swprintf_s(classname, L"%s%d", m_descriptor.window_name.c_str(), classNo++);

	WNDCLASSEXW wcex = {};
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = GetModuleHandle(nullptr);
	wcex.lpszClassName = classname;
	m_private->m_class_register = RegisterClassEx(&wcex);
	assert_win(m_private->m_class_register);

	DWORD dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	DWORD dwStyle = WS_OVERLAPPEDWINDOW; //| WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	RECT rect{ 0, 0, (LONG)m_descriptor.size.width, (LONG)m_descriptor.size.height };
	AdjustWindowRectEx(&rect, dwStyle, false, dwExStyle);
	m_private->m_window = CreateWindowExW(dwExStyle, (wchar_t*)MAKELONG(m_private->m_class_register, 0), m_descriptor.window_name.c_str(), dwStyle,
		CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, GetModuleHandle(nullptr), this);

	assert_win(m_private->m_window);

	ShowWindow(m_private->m_window, SW_SHOW);
	UpdateWindow(m_private->m_window);

	// vulkan setup
	vk::Win32SurfaceCreateInfoKHR surface_info;
	surface_info.setHwnd(m_private->m_window);
	surface_info.setHinstance(GetModuleHandle(nullptr));
	m_surface = context->m_instance.createWin32SurfaceKHRUnique(surface_info);

	m_swapchain = std::make_shared<Swapchain>();
	m_swapchain->setup(context, descriptor, m_surface.get());
}

cWindow::~cWindow()
{
	DestroyWindow(m_private->m_window);
	UnregisterClassW((wchar_t*)MAKELONG(m_private->m_class_register, 0), GetModuleHandle(nullptr));
}

LRESULT cWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	break;
	case WM_KEYDOWN:
	{
		m_input_worker.m_keyboard.m_state[wParam] = true;
	}
	break;
	case WM_KEYUP:
	{
		m_input_worker.m_keyboard.m_state[wParam] = false;
	}
	break;
	case WM_CHAR:
		m_input_worker.m_keyboard.m_char[m_input_worker.m_keyboard.m_char_count++] = wParam;
		break;

	case WM_MOUSEWHEEL:
	{
		m_input_worker.m_mouse.wheel = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
	}
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_LBUTTONDBLCLK:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		switch (uMsg)
		{
		case WM_LBUTTONDOWN:
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_LEFT].state |= cMouse::STATE_ON;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_LEFT].x = xPos;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_LEFT].y = yPos;
			break;
		case WM_LBUTTONUP:
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_LEFT].state = cMouse::STATE_OFF;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_LEFT].x = xPos;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_LEFT].y = yPos;
			break;
		case WM_MBUTTONDOWN:
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_MIDDLE].state |= cMouse::STATE_ON;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_MIDDLE].x = xPos;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_MIDDLE].y = yPos;
			break;
		case WM_MBUTTONUP:
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_MIDDLE].state = cMouse::STATE_OFF;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_MIDDLE].x = -1;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_MIDDLE].y = -1;
			break;
		case WM_RBUTTONDOWN:
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_RIGHT].state |= cMouse::STATE_ON;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_RIGHT].x = xPos;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_RIGHT].y = yPos;
			break;
		case WM_RBUTTONUP:
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_RIGHT].state = cMouse::STATE_OFF;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_RIGHT].x = -1;
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_RIGHT].y = -1;
			break;
		case WM_LBUTTONDBLCLK:
			m_input_worker.m_mouse.m_param[cMouse::BUTTON_LEFT].state = cMouse::STATE_ON;
			break;
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
			break;
		}
	}
	break;
	case WM_MOUSEMOVE:
	{
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		m_input_worker.m_mouse.xy = glm::ivec2(xPos, yPos);
	}
	break;
	case WM_CLOSE:
		m_is_close = true;
		break;
	case WM_QUIT:
	case WM_DESTROY:
		break;
	}
	return DefWindowProc(m_private->m_window, uMsg, wParam, lParam);

}
void cWindow::swap()
{

}
void cWindow::execute()
{
	{
		// holdは押しっぱなし
		m_input_worker.m_keyboard.m_is_on = m_input.m_keyboard.m_state & ~m_input.m_keyboard.m_state_old;
		m_input_worker.m_keyboard.m_is_off = m_input.m_keyboard.m_state_old & ~m_input.m_keyboard.m_state;
	}

	const auto& old = m_input.m_mouse;
	for (int i = 0; i < cMouse::BUTTON_NUM; i++)
	{
		auto& param = m_input_worker.m_mouse.m_param[i];
		auto& param_old = old.m_param[i];

		if (btr::isOn(param_old.state, cMouse::STATE_ON))
		{
			// ONは押したタイミングだけ立つ
			btr::setOff(param.state, cMouse::STATE_ON);
			param.time = 0.f;
		}
		if (btr::isOn(param_old.state, cMouse::STATE_HOLD))
		{
			// 押した時間を加算
			param.time += sGlobal::Order().getDeltaTime();
		}
		if (btr::isOn(param.state, cMouse::STATE_ON) || btr::isOn(param_old.state, cMouse::STATE_HOLD))
		{
			param.state |= cMouse::STATE_HOLD;
		}
		if (btr::isOn(param_old.state, cMouse::STATE_OFF))
		{
			param.state = 0;
			param.x = -1;
			param.y = -1;
			param.time = 0.f;
		}
	}
	m_input_worker.m_mouse.xy_old = old.xy;

	m_input = m_input_worker;
	m_input_worker.m_keyboard.m_state_old = m_input_worker.m_keyboard.m_state;
	m_input_worker.m_mouse.wheel = 0;
	m_input_worker.m_keyboard.m_char.fill(0);
	m_input_worker.m_keyboard.m_char_count = 0;

}

