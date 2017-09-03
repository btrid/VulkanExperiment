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

}


glm::uvec3 calcDipatchGroups(const glm::uvec3& num, const glm::uvec3& local_size)
{
	glm::uvec3 ret = (num + local_size - glm::uvec3(1)) / local_size;
	return ret;
}

}
