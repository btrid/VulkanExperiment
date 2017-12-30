#pragma once
#include <windowsx.h>
#include <vector>
#include <memory>
#include <unordered_map>

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cInput.h>
#include <btrlib/GPU.h>
//#include <btrlib/Module.h>
namespace btr
{
	struct Context;
}
struct RenderPassModule;

struct cWindowDescriptor
{
	std::wstring class_name;
	std::wstring window_name;

	vk::Extent2D size;
	vk::SurfaceFormatKHR surface_format_request;
	int backbuffer_num;
};
class cWindow
{
public:
private:
	struct Swapchain 
	{
		std::shared_ptr<btr::Context> m_context;
		vk::UniqueSwapchainKHR m_swapchain_handle;
		std::vector<vk::Image> m_backbuffer_image;

		vk::SurfaceFormatKHR m_surface_format;
		vk::Extent2D m_size;

		uint32_t m_backbuffer_index;

		vk::UniqueSemaphore m_swapbuffer_semaphore;
		vk::UniqueSemaphore m_submit_semaphore;

		Swapchain()
			: m_swapchain_handle()
			, m_backbuffer_index(0)
		{

		}

		void setup(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor, vk::SurfaceKHR surface);
		uint32_t swap();

		size_t getBackbufferNum()const { return m_backbuffer_image.size(); }
		vk::Extent2D getSize()const { return m_size; }
	};

	class Private
	{
	public:
		HWND		m_window;
		HINSTANCE	m_instance;


		Private() = default;
	};
	std::shared_ptr<Private> m_private;
protected:
	vk::UniqueSurfaceKHR m_surface;
	cInput m_input;
	Swapchain m_swapchain;
	cWindowDescriptor m_descriptor;
	std::shared_ptr<RenderPassModule> m_render_pass;

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
	cWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor);

	void sync()
	{
		MSG msg;
		auto old = m_input.m_mouse;
		m_input.m_mouse.wheel = 0;
		m_input.m_keyboard.m_char.fill(0);
		m_input.m_keyboard.m_char_count = 0;
		while (PeekMessage(&msg, m_private->m_window, 0, 0, PM_REMOVE))
		{
//			msgList.push_back(msg);			
//			for (auto&& msg : msgList)
			{
				switch (msg.message)
				{
				case WM_SYSKEYDOWN:
				{
					auto& p = m_input.m_keyboard.m_data[vk_alt];
					p.key = vk_alt;
					p.state = cKeyboard::STATE_ON;
				}
				break;
				case WM_SYSKEYUP:
				{
					auto& p = m_input.m_keyboard.m_data[vk_alt];
					p.key = vk_alt;
					p.state = cKeyboard::STATE_OFF;
				}
				break;
				case WM_KEYDOWN:
				{
					auto& p = m_input.m_keyboard.m_data[msg.wParam];
					p.key = (uint8_t)msg.wParam;
					p.state = cKeyboard::STATE_ON;
				}
				break;
				case WM_KEYUP:
				{
					auto& p = m_input.m_keyboard.m_data[msg.wParam];
					p.key = (uint8_t)msg.wParam;
					p.state = cKeyboard::STATE_OFF;
				}
				break;
				case WM_CHAR:
					m_input.m_keyboard.m_char[m_input.m_keyboard.m_char_count++] = msg.wParam;
					break;

				case WM_MOUSEWHEEL:
				{
					m_input.m_mouse.wheel = GET_WHEEL_DELTA_WPARAM(msg.wParam) / WHEEL_DELTA;
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
					int xPos = GET_X_LPARAM(msg.lParam);
					int yPos = GET_Y_LPARAM(msg.lParam);
					switch (msg.message)
					{
					case WM_LBUTTONDOWN:
						m_input.m_mouse.m_param[cMouse::BUTTON_LEFT].state |= cMouse::STATE_ON;
						m_input.m_mouse.m_param[cMouse::BUTTON_LEFT].x = xPos;
						m_input.m_mouse.m_param[cMouse::BUTTON_LEFT].y = yPos;
						break;
					case WM_LBUTTONUP:
						m_input.m_mouse.m_param[cMouse::BUTTON_LEFT].state = cMouse::STATE_OFF;
						m_input.m_mouse.m_param[cMouse::BUTTON_LEFT].x = xPos;
						m_input.m_mouse.m_param[cMouse::BUTTON_LEFT].y = yPos;
						break;
					case WM_MBUTTONDOWN:
						m_input.m_mouse.m_param[cMouse::BUTTON_MIDDLE].state |= cMouse::STATE_ON;
						m_input.m_mouse.m_param[cMouse::BUTTON_MIDDLE].x = xPos;
						m_input.m_mouse.m_param[cMouse::BUTTON_MIDDLE].y = yPos;
						break;
					case WM_MBUTTONUP:
						m_input.m_mouse.m_param[cMouse::BUTTON_MIDDLE].state = cMouse::STATE_OFF;
						m_input.m_mouse.m_param[cMouse::BUTTON_MIDDLE].x = -1;
						m_input.m_mouse.m_param[cMouse::BUTTON_MIDDLE].y = -1;
						break;
					case WM_RBUTTONDOWN:
						m_input.m_mouse.m_param[cMouse::BUTTON_RIGHT].state |= cMouse::STATE_ON;
						m_input.m_mouse.m_param[cMouse::BUTTON_RIGHT].x = xPos;
						m_input.m_mouse.m_param[cMouse::BUTTON_RIGHT].y = yPos;
						break;
					case WM_RBUTTONUP:
						m_input.m_mouse.m_param[cMouse::BUTTON_RIGHT].state = cMouse::STATE_OFF;
						m_input.m_mouse.m_param[cMouse::BUTTON_RIGHT].x = -1;
						m_input.m_mouse.m_param[cMouse::BUTTON_RIGHT].y = -1;
						break;
					case WM_LBUTTONDBLCLK:
						m_input.m_mouse.m_param[cMouse::BUTTON_LEFT].state = cMouse::STATE_ON;
						break;
					case WM_XBUTTONDOWN:
					case WM_XBUTTONUP:
						break;
					}
				}
				break;
				case WM_MOUSEMOVE:
				{
					int xPos = GET_X_LPARAM(msg.lParam);
					int yPos = GET_Y_LPARAM(msg.lParam);
					m_input.m_mouse.xy = glm::ivec2(xPos, yPos);
				}
				break;
				}
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}		

		for (auto& key : m_input.m_keyboard.m_data)
		{
			if (btr::isOn(key.state_old, cMouse::STATE_ON))
			{
				// ONは押したタイミングだけ立つ
				btr::setOff(key.state, cMouse::STATE_ON);
			}
			if (btr::isOn(key.state_old, cMouse::STATE_ON) || btr::isOn(key.state, cMouse::STATE_HOLD))
			{
				key.state |= cMouse::STATE_HOLD;
			}
			if (btr::isOn(key.state_old, cMouse::STATE_OFF))
			{
				key.state = 0;
			}
			key.state_old = key.state;
		}

		for (int i = 0; i < cMouse::BUTTON_NUM; i++)
		{
			auto& param = m_input.m_mouse.m_param[i];
			auto& param_old = old.m_param[i];
			
			if (btr::isOn(param_old.state, cMouse::STATE_ON))
			{
				// ONは押したタイミングだけ立つ
				btr::setOff(param.state, cMouse::STATE_ON);
				param.time = 0.f;
			}
			if(btr::isOn(param_old.state, cMouse::STATE_HOLD))
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
		m_input.m_mouse.xy_old = old.xy;

	}
public:

	glm::uvec2 getClientSize()const { return glm::uvec2(m_descriptor.size.width, m_descriptor.size.height); }
	template<typename T> T getClientSize()const { return T( m_descriptor.size.width, m_descriptor.size.height ); }
	const Swapchain& getSwapchain()const { return m_swapchain; }
	Swapchain& getSwapchain() { return m_swapchain; }
	vk::SurfaceKHR getSurface()const { return m_surface.get(); }
	const cInput& getInput()const { return m_input; }
	
	std::shared_ptr<RenderPassModule> getRenderBackbufferPass() { return m_render_pass; }

};

class sWindow : public Singleton<sWindow>
{
	friend Singleton<sWindow>;
	std::vector<std::weak_ptr<cWindow>> mWindowList;
public:
	template<typename T>
	std::shared_ptr<T> createWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& window_info)
	{
		auto window = std::make_shared<T>(context, window_info);
		mWindowList.emplace_back(window);
		return window;
	}

	void destroyWindow(cWindow* window)
	{
	}
};
