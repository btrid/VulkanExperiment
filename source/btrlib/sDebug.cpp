#include <btrlib/sDebug.h>
#include <btrlib/Define.h>
#include <btrlib/cStopWatch.h>

void sDebug::waitFence(const vk::Device& device, const vk::Fence& fence)const
{
	if (device.getFenceStatus(fence) != vk::Result::eSuccess) {
		cStopWatch timer;
		auto result = device.waitForFences(fence, VK_TRUE, 0xffffffffLLu);
		assert(result == vk::Result::eSuccess);
//		print(FLAG_PERFORMANCE, "wait_fence = %8.6fs\n", timer.getElapsedTimeAsSeconds());
	}
}
