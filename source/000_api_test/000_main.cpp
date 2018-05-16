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
	// �����������x���A���P�[�g���Ă��̌��ʂ�����
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
	// ����
	staging_memory.gc();
	count++;
}

void bittest()
{
	// count
	{
		uint64_t a = 0xffffffffffffffff;
		uint64_t c = 0;
		c = (a & 0x5555555555555555ull) + ((a >> 1) & 0x5555555555555555ull);
		c = (c & 0x3333333333333333ull) + ((c >> 2) & 0x3333333333333333ull);
		c = (c & 0x0f0f0f0f0f0f0f0full) + ((c >> 4) & 0x0f0f0f0f0f0f0f0full);
		c = (c & 0x00ff00ff00ff00ffull) + ((c >> 8) & 0x00ff00ff00ff00ffull);
		c = (c & 0x0000ffff0000ffffull) + ((c >> 16) & 0x0000ffff0000ffffull);
		c = (c & 0x00000000ffffffffull) + ((c >> 32) & 0x00000000ffffffffull);	
		c = c;
	}

	int x_shift = 7;
	int y_shift = 0;

	uint64_t x_line_mask = 0xffull & ~((1 << (8 - x_shift)) - 1);
	uint64_t x_mask_inv = x_line_mask | (x_line_mask << 8) | (x_line_mask << 16) | (x_line_mask << 24) | (x_line_mask << 32) | (x_line_mask << 40) | (x_line_mask << 48) | (x_line_mask << 56);
	uint64_t y_mask_inv = 0xffffffffffffffffull & ~((1ull << ((8 - y_shift) * 8)) - 1);
	auto x_mask = ~x_mask_inv;
	auto y_mask = ~y_mask_inv;
	{
		auto c1 = x_mask & y_mask;
		auto c2 = x_mask & (~y_mask);
		auto c3 = (~x_mask) & y_mask;
		auto c4 = (~x_mask) & (~y_mask);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				uint64_t map = (1ull << (y * 8 + x));
				if ((c1&map) != 0)
					printf("1");
				if ((c2&map) != 0)
					printf("2");
				if ((c3&map) != 0)
					printf("3");
				if ((c4&map) != 0)
					printf("4");
			}
			printf("\n");
		}
		printf("---\n");
	}

	{
		uint64_t x_line_mask2 = 0xffull & ((1 << (x_shift)) - 1);
		uint64_t x_mask_inv2 = x_line_mask2 | (x_line_mask2 << 8) | (x_line_mask2 << 16) | (x_line_mask2 << 24) | (x_line_mask2 << 32) | (x_line_mask2 << 40) | (x_line_mask2 << 48) | (x_line_mask2 << 56);
		uint64_t y_mask_inv2 = 0xffffffffffffffffllu & ((1ull << ((y_shift) * 8)) - 1);
		auto x_mask2 = ~x_mask_inv2;
		auto y_mask2 = ~y_mask_inv2;

		uint64_t map0 = 0xFFFFFFFFFFFFFFFFull;
		auto m1 = map0;
		m1 &= x_mask2 & y_mask2;
		m1 >>= y_shift*8;
		m1 >>= x_shift;

		auto m2 = map0;
		m2 &= x_mask2 & ~y_mask2;
		m2 <<= (8-y_shift)*8;
		m2 >>= x_shift;

		auto m3 = map0;
		m3 &= ~x_mask2 & y_mask2;
		m3 >>= y_shift*8;
		m3 <<= (8-x_shift);

		auto m4 = map0;
		m4 &= ~x_mask2 & ~y_mask2;
		m4 <<= (8-y_shift)*8;
		m4 <<= (8-x_shift);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				uint64_t map = (1ull << (y * 8 + x));
				if ((m1&map) != 0)
					printf("1");
				if ((m2&map) != 0)
					printf("2");
				if ((m3&map) != 0)
					printf("3");
				if ((m4&map) != 0)
					printf("4");
			}
			printf("\n");
		}
		printf("---\n");
		x_shift++;

	}
	x_shift++;

}
int main()
{
	bittest();
	uint32_t bit = 0;
	btr::setBit(bit, 1257, 16, 16);
	btr::setBit(bit, 1145, 0, 16);
	auto value = btr::getBit(bit, 0, 16);
	auto valuehi = btr::getBit(bit, 16, 16);

	glm::vec3 a(0.f, 0.f, 1.f);
	glm::vec3 b = glm::normalize(glm::vec3(1.f, 0.f, 1.f));
	auto angle = glm::degrees(glm::asin(glm::cross(a, b).y));
	auto mask = glm::u64vec3((1ull << 22ull) - 1, (1ull << 42ull) - 1, std::numeric_limits<uint64_t>::max());
	mask.z -= mask.y;
	mask.y -= mask.x;

	btr::setResourceAppPath("..\\..\\resource\\000_api_test\\");
//	ImageMemoryAllocate();
	memoryAllocate();
	memoryAllocater();


	return 0;
}

