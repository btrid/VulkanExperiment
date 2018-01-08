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
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		cWindow *pThis = NULL;
		if (uMsg == WM_NCCREATE)
		{
			CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
			pThis = (cWindow*)pCreate->lpCreateParams;
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
			pThis->m_private->m_window = hwnd;
		}
		else
		{
			pThis = (cWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
		}
		if (pThis)
		{
			return pThis->WindowProc(uMsg, wParam, lParam);
		}
		else
		{
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
	}

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
	cInput m_input_worker;
	Swapchain m_swapchain;
	cWindowDescriptor m_descriptor;
	std::shared_ptr<RenderPassModule> m_render_pass;

	bool m_is_close;
public:
	cWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& descriptor);
	~cWindow();
	void execute();
	void swap();
public:

	glm::uvec2 getClientSize()const { return glm::uvec2(m_descriptor.size.width, m_descriptor.size.height); }
	template<typename T> T getClientSize()const { return T( m_descriptor.size.width, m_descriptor.size.height ); }
	const Swapchain& getSwapchain()const { return m_swapchain; }
	Swapchain& getSwapchain() { return m_swapchain; }
	const cInput& getInput()const { return m_input; }
	
	std::shared_ptr<RenderPassModule> getRenderBackbufferPass() { return m_render_pass; }
	bool isClose()const {
		return m_is_close;
	}

private:
	LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class sWindow : public Singleton<sWindow>
{
	friend Singleton<sWindow>;
	std::vector<std::shared_ptr<cWindow>> mWindowList;
public:
	template<typename T>
	std::shared_ptr<T> createWindow(const std::shared_ptr<btr::Context>& context, const cWindowDescriptor& window_info)
	{
		assert(sThreadLocal::Order().getThreadIndex() == 0); // メインスレッド以外は対応してない
		auto window = std::make_shared<T>(context, window_info);
//		mWindowList.emplace_back(window);
		return window;
	}

	void destroyWindow(cWindow* window)
	{
	}
};
