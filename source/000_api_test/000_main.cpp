#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/App.h>

#pragma comment(lib, "applib.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "vulkan-1.lib")


void memoryAllocater()
{
	// メモリを何度もアロケートしてその結果を見る
	sGlobal::Order();
	auto device = sThreadLocal::Order().m_device[0];

	btr::AllocatedMemory staging_memory;
	staging_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached, 1024 * 1024 * 20);

	auto& thread_pool = sGlobal::Order().getThreadPool();
	struct Memory
	{
		int m_life;
		btr::BufferMemory m_alloced_memory;
	};
	std::vector<Memory> memory_list;
	int32_t next = 0;
	int32_t count = 10000;
	while (count >= 0)
	{
		if(next-- <= 0)
		{
			Memory m;
			m.m_life = std::rand() % 100;
			btr::AllocatedMemory::Descriptor desc;
			desc.size = std::rand() % 16 * 32 + 16;
			if (std::rand() % 100 < 10)
			{
				desc.attribute = btr::AllocatedMemory::AttributeFlagBits::SHORT_LIVE_BIT;
			}
			m.m_alloced_memory = staging_memory.allocateMemory(desc);
			memory_list.push_back(std::move(m));

			next = std::rand() % 100;
			if (memory_list.size() <= 7)
			{
				next = 0;
			}
			count--;
		}
		sGlobal::Order().sync();

		for (auto it = memory_list.begin(); it != memory_list.end();)
		{
			it->m_life--;
			if (it->m_life < 0) {
				it = memory_list.erase(it);
			}
			else {
				it++;
			}
		}
	}
	memory_list.clear();
	sGlobal::Order().sync();
	sGlobal::Order().sync();
	sGlobal::Order().sync();
	// 結果
	staging_memory.gc();
	count++;
}
int main()
{
	glm::vec3 a(0.f, 0.f, 1.f);
	glm::vec3 b = glm::normalize(glm::vec3(1.f, 0.f, 1.f));
	auto angle = glm::degrees(glm::asin(glm::cross(a, b).y));
	auto mask = glm::u64vec3((1ull << 22ull) - 1, (1ull << 42ull) - 1, std::numeric_limits<uint64_t>::max());
	mask.z -= mask.y;
	mask.y -= mask.x;

	btr::setResourceAppPath("..\\..\\resource\\000_api_test\\");
	memoryAllocater();


	return 0;
}

