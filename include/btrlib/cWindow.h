#pragma once
#include "Define.h"

#include <vector>
#include <memory>
#include <unordered_map>

#include <btrlib/Singleton.h>
#include <btrlib/sGlobal.h>
#include <btrlib/ThreadPool.h>
struct cKeybordInput {
	struct Param {
		char key;
		unsigned state;
	};
	std::unordered_map<WPARAM, Param> mData;
};

class cWindow
{
public:
	struct CreateInfo
	{
		cGPU gpu;
		std::wstring class_name;
		std::wstring window_name;

		vk::Extent2D size;
		vk::SurfaceFormatKHR surface_format_request;
		int backbuffer_num;
	};
private:
	struct Swapchain 
	{
		vk::SwapchainKHR m_swapchain_handle;
		std::vector<vk::Image> m_backbuffer_image;
		vk::SurfaceFormatKHR m_surface_format;
		cDevice m_use_device;
		uint32_t m_backbuffer_index;

		Swapchain()
			: m_swapchain_handle()
			, m_backbuffer_index(0)
		{

		}

		void setup(const CreateInfo& descriptor, vk::SurfaceKHR surface);
		uint32_t swap(vk::Semaphore& semaphore);
		size_t getSwapchainNum()const { return m_backbuffer_image.size(); }
	};

	class Private
	{
	public:
		HWND		m_window;
		HINSTANCE	m_instance;


		Private() = default;
	};
	std::shared_ptr<Private> m_private;

	vk::SurfaceKHR m_surface;
	cKeybordInput m_keybord;
	Swapchain m_swapchain;
	CreateInfo m_descriptor;

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_SYSKEYDOWN:
			break;

		case WM_SYSCHAR:
			break;

		case WM_SYSKEYUP:
			break;

		case WM_KEYDOWN:
//			wParam
			break;

		case WM_KEYUP:
			break;

		case WM_LBUTTONDBLCLK:
//			auto xPos = GET_XBUTTON_WPARAM(lParam);
//			auto yPos = GET_Y_LPARAM(lParam);
			break;
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

public:
	cWindow()
		: m_private(std::make_shared<Private>())
		, m_surface()
	{

	}

	void setup(const CreateInfo& descriptor)
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
		m_surface = sGlobal::Order().getVKInstance().createWin32SurfaceKHR(surfaceInfo);

		m_swapchain.setup(descriptor, m_surface);
	}

	void update()
	{
		MSG msg;
		std::vector<MSG> msgList;
		while (PeekMessage(&msg, m_private->m_window, 0, 0, PM_REMOVE))
		{
			msgList.push_back(msg);

			//if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
			{
//				TranslateMessage(&msg);
				switch (msg.message)
				{
				case WM_KEYDOWN:
				{
					auto& p = m_keybord.mData[msg.wParam];
					p.key = (char)msg.wParam;
					p.state = (unsigned)msg.lParam;
					u32 repeat = msg.lParam & 0xf;
					bool isPrevPress = (msg.lParam & (1 << 30)) != 0;
					repeat++;
				}
				break;

				case WM_KEYUP:
				{
					auto& p = m_keybord.mData[msg.wParam];
					p.key = (char)msg.wParam;
					p.state = (unsigned)msg.lParam;
					u32 repeat = msg.lParam & 0xf;
					bool isPrevPress = (msg.lParam & (1 << 30)) != 0;
					repeat++;
				}
				break;

				default:
					break;
				}
				// 作ったスレッドでしか動かない
				DispatchMessage(&msg);
			}
		}
		
		ThreadJob job;
		job.mJob.emplace_back([=]() 
		{
			for (auto&& msg : msgList)
			{
				DispatchMessage(&msg);
			}
		});

		job.mFinish = [&]() {
			// @Todo キーボードの更新など？
		};
//		btr::ThreadPool::Order().enque(job);

	}
public:

	glm::uvec2 getClientSize()const { return glm::uvec2(m_descriptor.size.width, m_descriptor.size.height); }
	const Swapchain& getSwapchain()const { return m_swapchain; }
	Swapchain& getSwapchain() { return m_swapchain; }
	vk::SurfaceKHR getSurface()const { return m_surface; }
};

class sWindow : public Singleton<sWindow>
{
	friend Singleton<sWindow>;
	std::vector<std::weak_ptr<cWindow>> mWindowList;

	std::shared_ptr<cWindow> createWindow()
	{
		auto window = std::make_shared<cWindow>();
		mWindowList.emplace_back(window);
		return window;
	}

	void destroyWindow(cWindow* window)
	{
//		window.reset();
	}

//	std::weak_ptr<cWindow> getWindow(HWND wnd) {
//		for
//	}
};
