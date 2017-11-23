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

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct AllocedImage
{
	vk::UniqueImage m_image;
	vk::UniqueDeviceMemory m_memory;
	void setup(const vk::ImageCreateInfo& info)
	{
//		info.
	}
};
void ImageMemoryAllocate()
{
	auto device = sGlobal::Order().getGPU(0).getDevice();
	btr::AllocatedMemory allocMemory;

	vk::BufferCreateInfo buffer_info;
	buffer_info.usage = vk::BufferUsageFlagBits::eStorageBuffer;
	buffer_info.size = 1024;
	auto buffer = device->createBufferUnique(buffer_info);

	vk::MemoryRequirements buffer_memory_request = device->getBufferMemoryRequirements(buffer.get());
	auto memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(device.getGPU(), buffer_memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::ImageCreateInfo image_info;
	image_info.imageType = vk::ImageType::e2D;
	image_info.format = vk::Format::eR8Uint;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = vk::SampleCountFlagBits::e1;
	image_info.tiling = vk::ImageTiling::eOptimal;
	image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
	image_info.sharingMode = vk::SharingMode::eExclusive;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.extent = { 256, 256, 1u };
	auto image = device->createImageUnique(image_info);

	vk::MemoryRequirements image_memory_request = device->getImageMemoryRequirements(image.get());
	vk::MemoryAllocateInfo image_memory_alloc_info;
	image_memory_alloc_info.allocationSize = image_memory_request.size;
	image_memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(device.getGPU(), image_memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
	auto image_memory = device->allocateMemoryUnique(image_memory_alloc_info);
	device->bindImageMemory(image.get(), image_memory.get(), 0);


}

struct AllocateInfo 
{
	vk::BufferCreateInfo m_info;
	vk::MemoryPropertyFlags m_memory_prop;
	vk::DeviceSize m_size;
};
void AllocateMemory(const cDevice& device, std::vector<AllocateInfo>&& alloc_info)
{
	std::vector<btr::AllocatedMemory> memory(alloc_info.size());
	for (size_t i = 0; i < alloc_info.size(); i++)
	{
		auto& info = alloc_info[i];
		auto buffer = device->createBufferUnique(info.m_info);
		vk::MemoryRequirements memory_request = device->getBufferMemoryRequirements(buffer.get());
	}

// 	vk::MemoryRequirements buffer_memory_request = device->getBufferMemoryRequirements(buffer.get());
// 	auto memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(device.getGPU(), buffer_memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);
//	return memory;

}
struct MemoryPool
{

};
void memoryAllocate()
{
	auto device = sGlobal::Order().getGPU(0).getDevice();

	vk::MemoryPropertyFlags host_memory = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostCached;
	vk::MemoryPropertyFlags device_memory = vk::MemoryPropertyFlagBits::eDeviceLocal;


	btr::AllocatedMemory a;
	a.setup(device, vk::BufferUsageFlagBits::eVertexBuffer, device_memory, 1000 * 1000 * 100);
	btr::AllocatedMemory vertex_memory;
	vertex_memory.setup(device, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 100);
	btr::AllocatedMemory uniform_memory;
	uniform_memory.setup(device, vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 20);
	btr::AllocatedMemory storage_memory;
	storage_memory.setup(device, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, device_memory, 1000 * 1000 * 200);
	btr::AllocatedMemory staging_memory;
	staging_memory.setup(device, vk::BufferUsageFlagBits::eTransferSrc, host_memory, 1000 * 1000 * 100);

	vk::ImageCreateInfo image_info;
	image_info.imageType = vk::ImageType::e2D;
	image_info.format = vk::Format::eR8Uint;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = vk::SampleCountFlagBits::e1;
	image_info.tiling = vk::ImageTiling::eOptimal;
	image_info.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;
	image_info.sharingMode = vk::SharingMode::eExclusive;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.extent = { 256, 256, 1u };
	auto image = device->createImage(image_info);

	vk::MemoryRequirements memory_request = device->getImageMemoryRequirements(image);
	vk::MemoryAllocateInfo memory_alloc_info;
	memory_alloc_info.allocationSize = memory_request.size;
	memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

	auto memory_prop = device.getGPU().getMemoryProperties();

	int _a = 0;
}
void memoryAllocater()
{
	// メモリを何度もアロケートしてその結果を見る
	sGlobal::Order();
	auto device = sGlobal::Order().getGPU(0).getDevice();

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
			btr::BufferMemoryDescriptor desc;
			desc.size = std::rand() % 16 * 32 + 16;
			if (std::rand() % 100 < 10)
			{
				desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
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

float calcDepth(float v, float n, float f)
{
	return /*1.f -*/ (n * f / (v * (f - n) - f));
}

void depth_test()
{

	auto p = glm::perspective(glm::radians(60.f), 1.f, 0.1f, 5000.f);
	auto v = glm::lookAt(glm::vec3(2888.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
	auto s = p*v*vec4(30.f, 10.f, -15.f, 1.f);
	s /= s.w;

	auto d = calcDepth(4500.f, 0.1f, 5000.f);
	int a = 0;
	a++;
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
//	ImageMemoryAllocate();
	depth_test();
	memoryAllocate();
	memoryAllocater();


	return 0;
}

