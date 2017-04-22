#pragma once

#include <btrlib/Define.h>
struct MemoryAllocatorPrivate
{
	vk::DeviceMemory m_memory;
public:
private:

};
class MemoryAllocator
{
public:
	MemoryAllocator(vk::PhysicalDevice gpu)
	{
	}

//	void setup();

	std::shared_ptr<MemoryAllocatorPrivate> m_private;
}

