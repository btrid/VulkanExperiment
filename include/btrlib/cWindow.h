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
		ATOM		m_class_register;
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
	~cWindow();
	void sync();
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
		assert(sThreadLocal::Order().getThreadIndex() == 0); // メインスレッド以外は対応してない
		auto window = std::make_shared<T>(context, window_info);
		mWindowList.emplace_back(window);
		return window;
	}

	void destroyWindow(cWindow* window)
	{
	}
};
