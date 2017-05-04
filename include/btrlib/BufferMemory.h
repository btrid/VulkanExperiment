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
	vk::BufferMemoryBarrier m_memory_barrier;
	struct Resource
	{
		Zone m_zone;
		vk::DeviceMemory m_memory_ref;
		void* m_mapped_memory;
	};
	std::shared_ptr<Resource> m_resource;
public:
	vk::DescriptorBufferInfo getBufferInfo()const { return m_buffer_info; }
	vk::Buffer getBuffer()const { return m_buffer_info.buffer; }
	vk::DeviceSize getSize()const { return m_buffer_info.range; }
	vk::DeviceSize getOffset()const { return m_buffer_info.offset; }
	vk::DeviceMemory getDeviceMemory()const { return m_resource->m_memory_ref; }
	void* getMappedPtr()const { return m_resource->m_mapped_memory; }
	template<typename T> T* getMappedPtr(size_t offset_num = 0)const { return static_cast<T*>(m_resource->m_mapped_memory)+offset_num; }

	const vk::BufferMemoryBarrier& makeMemoryBarrier(vk::AccessFlags srcAccessMask) {
		m_memory_barrier.buffer = m_buffer_info.buffer;
		m_memory_barrier.size = m_buffer_info.range;
		m_memory_barrier.offset = m_buffer_info.offset;
		m_memory_barrier.dstAccessMask = m_memory_barrier.srcAccessMask;
		m_memory_barrier.srcAccessMask = srcAccessMask;
		return m_memory_barrier;
	}
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

		vk::BufferCreateInfo m_bufer_info;
		void* m_mapped_memory;

		std::string m_name;
		~Resource()
		{
		}
	};
	std::shared_ptr<Resource> m_resource;

	bool isValid()const { return m_resource.get(); }
	void setName(const std::string& name) { m_resource->m_name = name; }
	void setup(const cDevice& device, vk::BufferUsageFlags flag, vk::MemoryPropertyFlags memory_type, vk::DeviceSize size)
	{
		assert(!m_resource);
		btr::setOff(memory_type, vk::MemoryPropertyFlagBits::eDeviceLocal);
		memory_type |= vk::MemoryPropertyFlagBits::eHostVisible;

		auto resource = std::make_shared<Resource>();
		resource->m_device = device;

		vk::BufferCreateInfo buffer_info;
		buffer_info.usage = flag;
		buffer_info.size = size;
		resource->m_buffer = device->createBuffer(buffer_info);
		resource->m_bufer_info = buffer_info;

		vk::MemoryRequirements memory_request = resource->m_device->getBufferMemoryRequirements(resource->m_buffer);
		vk::MemoryAllocateInfo memory_alloc;
		memory_alloc.setAllocationSize(memory_request.size);
		memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, memory_type));
		resource->m_memory = device->allocateMemory(memory_alloc);
		device->bindBufferMemory(resource->m_buffer, resource->m_memory, 0);

		resource->m_free_zone.setup(size, memory_request.alignment);

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


	enum class AttributeFlagBits : uint32_t
	{
		SHORT_LIVE_BIT = 1<<0,
		MEMORY_UPDATE_ALL_PER_FRAME_BIT = 1 << 1,
		MEMORY_CONSTANT = 1 << 2,
	};
	using AttributeFlags = vk::Flags<AttributeFlagBits, uint32_t>;
	struct Argument
	{
		vk::DeviceSize size;
		AttributeFlags attribute;
		Argument()
			: size(0)
			, attribute(AttributeFlags())
		{}
	};
	AllocatedMemory allocateMemory(vk::DeviceSize size)
	{
		Argument arg;
		arg.size = size;
		arg.attribute = AttributeFlags();
		return allocateMemory(arg);
	}
	AllocatedMemory allocateMemory(Argument arg)
	{
		auto zone = m_resource->m_free_zone.alloc(arg.size);
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
		alloc.m_buffer_info.range = arg.size;

		alloc.m_resource->m_mapped_memory = nullptr;
		if (m_resource->m_mapped_memory)
		{
			alloc.m_resource->m_mapped_memory = (char*)m_resource->m_mapped_memory + alloc.m_resource->m_zone.m_start;
		}
		return alloc;
	}
	cDevice getDevice()const { return m_resource->m_device; }
	vk::BufferCreateInfo getBufferCreateInfo()const { return m_resource->m_bufer_info; }

};

template<typename T>
struct UpdateBuffer
{
	AllocatedMemory m_device_memory;
	AllocatedMemory m_staging_memory;
	vk::DeviceSize m_begin;
	vk::DeviceSize m_end;
	uint32_t m_frame;
	void setup(btr::BufferMemory device_memory, btr::BufferMemory staging_memory)
	{
		assert(device_memory.isValid());
		assert(btr::isOn(device_memory.getBufferCreateInfo().usage, vk::BufferUsageFlagBits::eTransferDst));
		assert(staging_memory.isValid());
		assert(btr::isOn(staging_memory.getBufferCreateInfo().usage, vk::BufferUsageFlagBits::eTransferSrc));
		m_device_memory = device_memory.allocateMemory(sizeof(T));
		m_staging_memory = staging_memory.allocateMemory(sizeof(T)*sGlobal::FRAME_MAX);
		m_begin = ~vk::DeviceSize(0);
		m_end = vk::DeviceSize(0);
		m_frame = 0;
	}

	void subupdate(const T& data)
	{
		auto* ptr = m_staging_memory.getMappedPtr<T>(m_frame);
		*ptr = data;
		m_begin = 0;
		m_end = sizeof(T);
	}
	void subupdate(void* data, vk::DeviceSize data_size, vk::DeviceSize offset)
	{
		auto* ptr = m_staging_memory.getMappedPtr<T>(m_frame);
		memcpy_s(ptr + offset, data_size, data, data_size);
		m_begin = std::min(offset, m_begin);
		m_end = std::max(offset + data_size, m_end);
	}
	void update(vk::CommandBuffer cmd)
	{
		if (m_end < m_begin) {
			return;
		}
		vk::BufferCopy copy_info;
		copy_info.setSize(m_end - m_begin);
		copy_info.setSrcOffset(m_staging_memory.getOffset() + sizeof(T)*m_frame + m_begin);
		copy_info.setDstOffset(m_device_memory.getOffset() + m_begin);
		cmd.copyBuffer(m_staging_memory.getBuffer(), m_device_memory.getBuffer(), copy_info);

		m_begin = ~vk::DeviceSize(0);
		m_end = vk::DeviceSize(0);
		m_frame = (m_frame + 1) % sGlobal::FRAME_MAX;

	}
//	T* getPtr() { return m_staging_memory.getMappedPtr<T>(m_frame); }

	vk::DescriptorBufferInfo getBufferInfo()const { return m_device_memory.getBufferInfo(); }
	AllocatedMemory& getAllocateMemory() { return m_device_memory; }
	const AllocatedMemory& getAllocateMemory()const { return m_device_memory; }
	vk::DeviceMemory getMemory()const { return m_staging_memory.getDeviceMemory(); }
};

}
