#pragma once

#include <vector>
#include <mutex>
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
namespace btr
{
struct Zone
{
	vk::DeviceSize m_start;
	vk::DeviceSize m_end;

	Zone()
		: m_start(0llu)
		, m_end(0llu)
	{}

	Zone split(vk::DeviceSize size)
	{
		Zone newZone;
		newZone.m_start = m_start;
		newZone.m_end = m_start + size;

		m_start += size;

		return newZone;
	}
	void marge(const Zone& zone)
	{
		assert(m_start == zone.m_end);
		m_start = zone.m_start;
	}

	bool tryMarge(const Zone zone)
	{
		if (m_start != zone.m_end)
		{
			return false;
		}
		marge(zone);
		return true;
	}

	vk::DeviceSize range()const { return m_end - m_start; }
	bool isValid()const { return m_end != 0llu; }
};
struct FreeZone
{
	std::vector<Zone> m_free_zone;
	std::vector<Zone> m_active_zone;

	vk::DeviceSize m_align;
	std::mutex m_mutex;
	~FreeZone()
	{
		assert(m_active_zone.empty());
	}
	void setup(vk::DeviceSize size, vk::DeviceSize align)
	{
		assert(m_free_zone.empty());

		m_align = align;
		Zone zone;
		zone.m_start = 0;
		zone.m_end = size;

		std::lock_guard<std::mutex> lock(m_mutex);
		m_free_zone.push_back(zone);
	}
	Zone alloc(vk::DeviceSize size)
	{
		size = btr::align(size, m_align);
		std::lock_guard<std::mutex> lock(m_mutex);
		for (auto& zone : m_free_zone)
		{
			if (zone.range() >= size)
			{
				m_active_zone.emplace_back(zone.split(size));
				return m_active_zone.back();
			}
		}

		// sortしてもう一度
		sort();
		for (auto& zone : m_free_zone)
		{
			if (zone.range() >= size)
			{
				m_active_zone.emplace_back(zone.split(size));
				return m_active_zone.back();
			}
		}
		return Zone();
	}

	void free(const Zone& zone)
	{
		assert(zone.isValid());

		std::lock_guard<std::mutex> lock(m_mutex);
		{
			// activeから削除
			auto it = std::find_if(m_active_zone.begin(), m_active_zone.end(), [&](Zone& active) { return active.m_start == zone.m_start; });
			m_active_zone.erase(it);
		}

		{
			// freeにマージ
			for (auto& free_zone : m_free_zone)
			{
				if (free_zone.tryMarge(zone)) {
					return;
				}
			}

			// できなければ、新しい領域として追加
			m_free_zone.emplace_back(zone);
		}

	}
private:
	void sort()
	{
		std::sort(m_free_zone.begin(), m_free_zone.end(), [](Zone& a, Zone& b) { return a.m_start < b.m_start; });
		for (size_t i = m_free_zone.size() - 1; i > 0; i--)
		{
			if (m_free_zone[i - 1].tryMarge(m_free_zone[i])) {
				m_free_zone.erase(m_free_zone.begin() + i);
			}
		}
	}
};
struct AllocatedMemory
{
	vk::DescriptorBufferInfo m_buffer_info;
	struct Resource
	{
		Zone m_zone;
		vk::DeviceMemory m_memory_ref;
		void* m_mapped_memory;
	};

	std::shared_ptr<Resource> m_resource;
	vk::DescriptorBufferInfo getBufferInfo()const { return m_buffer_info; }
	vk::Buffer getBuffer()const { return m_buffer_info.buffer; }
	vk::DeviceSize getSize()const { return m_buffer_info.range; }
	vk::DeviceSize getOffset()const { return m_buffer_info.offset; }
	vk::DeviceMemory getDeviceMemory()const { return m_resource->m_memory_ref; }
	void* getMappedPtr()const { return m_resource->m_mapped_memory; }
};
struct BufferMemory
{
	struct Resource
	{
		cDevice m_device;
		vk::Buffer m_buffer;

		vk::DeviceMemory m_memory;
		vk::DeviceSize m_allocate_size;
		FreeZone m_free_zone;

		vk::MemoryRequirements m_memory_request;
		vk::MemoryAllocateInfo m_memory_alloc;
		vk::MemoryType m_memory_type;

		void* m_mapped_memory;
		~Resource()
		{
		}
	};
	std::shared_ptr<Resource> m_resource;

	bool isValid()const { return m_resource.get(); }

	void setup(const cDevice& device, vk::BufferUsageFlags flag, vk::MemoryPropertyFlags memory_type, vk::DeviceSize size)
	{
		auto resource = std::make_shared<Resource>();
		device.getGPU();
		resource->m_device = device;

		auto limits = device.getGPU().getProperties().limits;
		vk::DeviceSize align = 16;
		if (btr::isOn(flag, vk::BufferUsageFlagBits::eStorageBuffer)) {
			align = std::max(align, limits.minStorageBufferOffsetAlignment);
		}
		if (btr::isOn(flag, vk::BufferUsageFlagBits::eUniformBuffer)) {
			align = std::max(align, limits.minUniformBufferOffsetAlignment);
		}
		resource->m_free_zone.setup(size, align);
		vk::BufferCreateInfo buffer_info;
		buffer_info.usage = flag;
		buffer_info.size = size;
		resource->m_buffer = device->createBuffer(buffer_info);

		vk::MemoryRequirements memory_request = resource->m_device->getBufferMemoryRequirements(resource->m_buffer);
		vk::MemoryAllocateInfo memory_alloc;
		memory_alloc.setAllocationSize(memory_request.size);
		memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, memory_type));
		resource->m_memory = device->allocateMemory(memory_alloc);
		device->bindBufferMemory(resource->m_buffer, resource->m_memory, 0);

		resource->m_memory_alloc = memory_alloc;
		resource->m_memory_request = memory_request;
		auto memory_prop = device.getGPU().getMemoryProperties();
		resource->m_memory_type = memory_prop.memoryTypes[memory_alloc.memoryTypeIndex];

		resource->m_mapped_memory = nullptr;
		if (btr::isOn(resource->m_memory_type.propertyFlags, vk::MemoryPropertyFlagBits::eHostCoherent) )
		{
			resource->m_mapped_memory = device->mapMemory(resource->m_memory, 0, size);
		}
		m_resource = std::move(resource);
	}

	// @deprecaterd
	void setup(const cDevice& device, vk::BufferUsageFlags flag, vk::DeviceSize size)
	{
		setup(device, flag, vk::MemoryPropertyFlagBits::eDeviceLocal, size);
	}

	AllocatedMemory allocateMemory(vk::DeviceSize size)
	{
		auto zone = m_resource->m_free_zone.alloc(size);
		assert(zone.isValid());

		AllocatedMemory alloc;
		auto deleter = [&](AllocatedMemory::Resource* ptr)
		{
			m_resource->m_free_zone.free(ptr->m_zone);
			delete ptr;
		};
		alloc.m_resource = std::shared_ptr<AllocatedMemory::Resource>(new AllocatedMemory::Resource, deleter);
		alloc.m_resource->m_zone = zone;
		alloc.m_resource->m_memory_ref = m_resource->m_memory;
		alloc.m_buffer_info.buffer = m_resource->m_buffer;
		alloc.m_buffer_info.offset = alloc.m_resource->m_zone.m_start;
		alloc.m_buffer_info.range = size;

		alloc.m_resource->m_mapped_memory = nullptr;
		if (m_resource->m_mapped_memory)
		{
			alloc.m_resource->m_mapped_memory = (char*)m_resource->m_mapped_memory + alloc.m_resource->m_zone.m_start;
		}
		return alloc;
	}
	cDevice getDevice()const { return m_resource->m_device; }

};

template<typename T>
struct UniformBufferEx
{
	AllocatedMemory m_buffer;
	size_t update_begin;
	size_t update_end;

	struct Resource
	{
		vk::Device m_device;
		vk::Buffer m_staging_buffer;
		vk::DeviceMemory m_staging_memory;
		char* m_staging_data;

		~Resource()
		{
			m_device.unmapMemory(m_staging_memory);

		}
	};
	std::shared_ptr<Resource> m_resource;
	void setup(BufferMemory& memory) 
	{
		auto size = sizeof(T);
		m_buffer = memory.allocateBuffer(size);
		m_resource = std::make_shared<Resource>();

		{
			// host_visibleなバッファの作成
			vk::BufferCreateInfo buffer_info;
			buffer_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
			buffer_info.size = size;

			m_resource->m_staging_buffer = memory.getDevice()->createBuffer(buffer_info);
			m_resource->m_device = memory.getDevice().getHandle();
			vk::MemoryRequirements memory_request = memory.getDevice()->getBufferMemoryRequirements(m_resource->m_staging_buffer);
			vk::MemoryAllocateInfo memory_alloc;
			memory_alloc.setAllocationSize(memory_request.size);
			memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(memory.getDevice().getGPU(), memory_request, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
			m_resource->m_staging_memory = memory.getDevice()->allocateMemory(memory_alloc);

			m_resource->m_staging_data = (char*)memory.getDevice()->mapMemory(m_resource->m_staging_memory, 0, buffer_info.size, vk::MemoryMapFlags());

			memory.getDevice()->bindBufferMemory(m_resource->m_staging_buffer, m_resource->m_staging_memory, 0);

			update_end = 0;
			update_begin = std::numeric_limits<size_t>::max();
		}
	}

	void subupdate(const T& data)
	{
		subupdate(data, sizeof(T), 0);
	}
	void subupdate(void* data, vk::DeviceSize data_size, vk::DeviceSize offset)
	{
		memcpy_s(m_resource->m_staging_data + offset, data_size, data, data_size);
		update_begin = glm::min(update_begin, offset);
		update_end = glm::max(update_end, offset + data_size);
	}
	void buildCmd(vk::CommandBuffer cmd)
	{
		if (update_end < update_begin)
		{
			return;
		}

		vk::BufferCopy copy;
		copy.size = update_end - update_begin;
		copy.dstOffset = update_begin;
		copy.srcOffset = update_begin;
		cmd.copyBuffer(m_resource->m_staging_buffer, m_buffer.getBuffer(), copy);
	}
};

}
