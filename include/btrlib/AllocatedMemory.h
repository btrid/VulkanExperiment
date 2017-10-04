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
	uint32_t m_is_reverse_alloc : 1;
	uint32_t _flag : 31;
	GameFrame m_wait_frame;	//!< 遅延削除のために数フレーム待つ
	Zone()
		: m_start(0llu)
		, m_end(0llu)
		, m_is_reverse_alloc(0u)
	{}

	Zone split(vk::DeviceSize size)
	{
		Zone newZone;
		newZone.m_start = m_start;
		newZone.m_end = m_start + size;
		newZone.m_is_reverse_alloc = 0;
		m_start += size;

		return newZone;
	}
	Zone splitReverse(vk::DeviceSize size)
	{
		Zone newZone;
		newZone.m_end = m_end;
		newZone.m_start = m_end - size;
		newZone.m_is_reverse_alloc = 1;
		m_end -= size;
		return newZone;
	}
	void marge(const Zone& zone)
	{
		assert(m_start == zone.m_end || m_end == zone.m_start);
		if (m_start == zone.m_end)
		{
			m_start = zone.m_start;
		}
		else 
		{
			m_end = zone.m_end;
		}
		assert(isValid());
		assert(range() != 0);
	}

	bool tryMarge(const Zone zone)
	{
		if (m_start != zone.m_end && m_end != zone.m_start)
		{
			return false;
		}
		marge(zone);
		return true;
	}

	vk::DeviceSize range()const { return m_end - m_start; }
	bool isValid()const { return m_end != 0llu; }
};

/**
 * GPUメモリプール
 * オレオレ実装
 * メモリ使用率重視なので、速度重視のものも必要かもしれない
 */
struct GPUMemoryAllocater
{
	struct DelayedFree {
		std::vector<Zone> m_list;
		std::mutex m_mutex;
	};
	std::vector<Zone> m_free_zone;
	std::vector<Zone> m_active_zone;
	DelayedFree m_delayed_active;
	vk::DeviceSize m_align;		//!< メモリのアラインメント
	GameFrame m_last_gc;		//!< 最後に実行したgcのフレーム

	std::mutex m_free_zone_mutex;

	~GPUMemoryAllocater()
	{
		assert(m_active_zone.empty());
	}
	void setup(vk::DeviceSize size, vk::DeviceSize align)
	{
		assert(m_free_zone.empty());
		m_last_gc = sGlobal::Order().getGameFrame();
		m_align = align;
		Zone zone;
		zone.m_start = 0;
		zone.m_end = size;

		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
		m_free_zone.push_back(zone);
	}
	Zone alloc(vk::DeviceSize size, bool is_reverse)
	{
		if (is_reverse)
		{
			return allocReverse(size);
		}

		if (sGlobal::Order().isElapsed(m_last_gc, 1u))
		{
			gc_impl();
		}

		size = btr::align(size, m_align);
		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
		for (auto& zone : m_free_zone)
		{
			if (zone.range() >= size)
			{
				m_active_zone.emplace_back(zone.split(size));
				assert(m_active_zone.back().range() != 0);
				return m_active_zone.back();
			}
		}
		return Zone();
	}
	Zone allocReverse(vk::DeviceSize size)
	{
		if (sGlobal::Order().isElapsed(m_last_gc, 1u))
		{
			gc_impl();
		}
		size = btr::align(size, m_align);
		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
		for (auto it = m_free_zone.rbegin(); it != m_free_zone.rend(); it++)
		{
			auto& zone = *it;
			if (zone.range() >= size)
			{
				m_active_zone.emplace_back(zone.splitReverse(size));
				assert(m_active_zone.back().range() != 0);
				return m_active_zone.back();
			}
		}
		return Zone();

	}

	void delayedFree(Zone zone)
	{
		assert(zone.isValid());
		zone.m_wait_frame = sGlobal::Order().getGameFrame();

		{
			std::lock_guard<std::mutex> lock(m_free_zone_mutex);
			auto it = std::find_if(m_active_zone.begin(), m_active_zone.end(), [&](Zone& active) { return active.m_start == zone.m_start; });
			if (it != m_active_zone.end())
			{
				m_active_zone.erase(it);
			}

		}

		auto& list = m_delayed_active;
		std::lock_guard<std::mutex> lock(list.m_mutex);
		list.m_list.push_back(zone);

	}
	void free(const Zone& zone)
	{
		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
		free_impl(zone);

	}

	void gc()
	{
		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
		gc_impl();
	}
private:
	void free_impl(const Zone& zone)
	{
		assert(zone.isValid());
		assert(zone.range() != 0);
		{
			// activeから削除
			auto it = std::find_if(m_active_zone.begin(), m_active_zone.end(), [&](Zone& active) { return active.m_start == zone.m_start; });
			if (it != m_active_zone.end()) {
				m_active_zone.erase(it);
			}
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
	void gc_impl()
	{
		{
			auto& list = m_delayed_active;
			std::lock_guard<std::mutex> lock(list.m_mutex);
			for (auto it = list.m_list.begin(); it != list.m_list.end();)
			{
				if (sGlobal::Order().isElapsed(it->m_wait_frame))
				{
					// cmdが発行されたので削除
					free_impl(*it);
					it = list.m_list.erase(it);
				}
				else {
					it++;
				}
			}
		}

		std::sort(m_free_zone.begin(), m_free_zone.end(), [](Zone& a, Zone& b) { return a.m_start < b.m_start; });
		for (size_t i = m_free_zone.size() - 1; i > 0; i--)
		{
			if (m_free_zone[i - 1].tryMarge(m_free_zone[i])) {
				m_free_zone.erase(m_free_zone.begin() + i);
			}
		}
		m_last_gc = sGlobal::Order().getGameFrame();
	}
};
struct BufferMemory
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
	bool isValid()const { return !!m_resource; }
	vk::DescriptorBufferInfo getBufferInfo()const { return m_buffer_info; }
	vk::DeviceMemory getDeviceMemory()const { return m_resource->m_memory_ref; }
	void* getMappedPtr()const { return m_resource->m_mapped_memory; }
	template<typename T> T* getMappedPtr(size_t offset_num = 0)const { return static_cast<T*>(m_resource->m_mapped_memory)+offset_num; }

	const vk::BufferMemoryBarrier& makeMemoryBarrier(vk::AccessFlags barrier) {
		m_memory_barrier.buffer = m_buffer_info.buffer;
		m_memory_barrier.size = m_buffer_info.range;
		m_memory_barrier.offset = m_buffer_info.offset;
		m_memory_barrier.srcAccessMask = m_memory_barrier.dstAccessMask;
		m_memory_barrier.dstAccessMask = barrier;
		return m_memory_barrier;
	}
	vk::BufferMemoryBarrier makeMemoryBarrierEx() {
		vk::BufferMemoryBarrier barrier;
		barrier.buffer = m_buffer_info.buffer;
		barrier.size = m_buffer_info.range;
		barrier.offset = m_buffer_info.offset;
		return barrier;
	}
};

struct ImageMemory
{
//	vk::MemoryRequirements
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
	bool isValid()const { return !!m_resource; }
	vk::DescriptorBufferInfo getBufferInfo()const { return m_buffer_info; }
	vk::DeviceMemory getDeviceMemory()const { return m_resource->m_memory_ref; }
	void* getMappedPtr()const { return m_resource->m_mapped_memory; }
	template<typename T> T* getMappedPtr(size_t offset_num = 0)const { return static_cast<T*>(m_resource->m_mapped_memory) + offset_num; }

	const vk::BufferMemoryBarrier& makeMemoryBarrier(vk::AccessFlags barrier) {
		m_memory_barrier.buffer = m_buffer_info.buffer;
		m_memory_barrier.size = m_buffer_info.range;
		m_memory_barrier.offset = m_buffer_info.offset;
		m_memory_barrier.srcAccessMask = m_memory_barrier.dstAccessMask;
		m_memory_barrier.dstAccessMask = barrier;
		return m_memory_barrier;
	}
	vk::BufferMemoryBarrier makeMemoryBarrierEx() {
		vk::BufferMemoryBarrier barrier;
		barrier.buffer = m_buffer_info.buffer;
		barrier.size = m_buffer_info.range;
		barrier.offset = m_buffer_info.offset;
		return barrier;
	}
};
struct AllocatedMemory
{
	struct Resource
	{
		cDevice m_device;
		vk::UniqueBuffer m_buffer;
		vk::UniqueDeviceMemory m_memory;

		GPUMemoryAllocater m_free_zone;

		vk::MemoryRequirements m_memory_request;
		vk::MemoryAllocateInfo m_memory_alloc;
		vk::MemoryType m_memory_type;

		vk::BufferCreateInfo m_buffer_info;
		void* m_mapped_memory;

		std::string m_name;
		~Resource()
		{
		}
	};
	std::shared_ptr<Resource> m_resource;

	void gc() { m_resource->m_free_zone.gc(); }
	bool isValid()const { return m_resource.get(); }
	void setName(const std::string& name) { m_resource->m_name = name; }
	void setup(const cDevice& device, vk::BufferUsageFlags flag, vk::MemoryPropertyFlags memory_type, vk::DeviceSize size)
	{
		assert(!m_resource);

		auto resource = std::make_shared<Resource>();
		resource->m_device = device;

		vk::BufferCreateInfo buffer_info;
		buffer_info.usage = flag;
		buffer_info.size = size;
		resource->m_buffer = device->createBufferUnique(buffer_info);
		resource->m_buffer_info = buffer_info;

		vk::MemoryRequirements memory_request = resource->m_device->getBufferMemoryRequirements(resource->m_buffer.get());
		vk::MemoryAllocateInfo memory_alloc;
		memory_alloc.setAllocationSize(memory_request.size);
		memory_alloc.setMemoryTypeIndex(cGPU::Helper::getMemoryTypeIndex(device.getGPU(), memory_request, memory_type));
		resource->m_memory = device->allocateMemoryUnique(memory_alloc);
		device->bindBufferMemory(resource->m_buffer.get(), resource->m_memory.get(), 0);

		resource->m_free_zone.setup(size, memory_request.alignment);

		resource->m_memory_alloc = memory_alloc;
		resource->m_memory_request = memory_request;
		auto memory_prop = device.getGPU().getMemoryProperties();
		resource->m_memory_type = memory_prop.memoryTypes[memory_alloc.memoryTypeIndex];

		resource->m_mapped_memory = nullptr;
		if (btr::isOn(resource->m_memory_type.propertyFlags, vk::MemoryPropertyFlagBits::eHostCoherent) )
		{
			resource->m_mapped_memory = device->mapMemory(resource->m_memory.get(), 0, size);
		}
		m_resource = std::move(resource);
	}


	enum class AttributeFlagBits : uint32_t
	{
		SHORT_LIVE_BIT = 1<<0,
	};
	using AttributeFlags = vk::Flags<AttributeFlagBits, uint32_t>;
	struct Descriptor
	{
		vk::DeviceSize size;
		AttributeFlags attribute;
		Descriptor()
			: size(0)
			, attribute(AttributeFlags())
		{}
	};
	BufferMemory allocateMemory(vk::DeviceSize size)
	{
		Descriptor arg;
		arg.size = size;
		arg.attribute = AttributeFlags();
		return allocateMemory(arg);
	}
	BufferMemory allocateMemory(Descriptor arg)
	{

		// size0はおかしいよね
		assert(arg.size != 0);

		auto zone = m_resource->m_free_zone.alloc(arg.size, btr::isOn(arg.attribute, AttributeFlagBits::SHORT_LIVE_BIT));

		// allocできた？
		assert(zone.isValid());

		BufferMemory alloc;
		auto deleter = [&](BufferMemory::Resource* ptr)
		{
			m_resource->m_free_zone.delayedFree(ptr->m_zone);
			delete ptr;
		};
		alloc.m_resource = std::shared_ptr<BufferMemory::Resource>(new BufferMemory::Resource, deleter);
		alloc.m_resource->m_zone = zone;
		alloc.m_resource->m_memory_ref = m_resource->m_memory.get();
		alloc.m_buffer_info.buffer = m_resource->m_buffer.get();
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
	const vk::BufferCreateInfo& getBufferCreateInfo()const { return m_resource->m_buffer_info; }
	vk::Buffer getBuffer()const { return m_resource->m_buffer.get(); }

};

struct UpdateBufferDescriptor
{
	btr::AllocatedMemory device_memory;
	btr::AllocatedMemory staging_memory;
	uint32_t frame_max;
	uint32_t element_num;

	UpdateBufferDescriptor()
	{
		frame_max = -1;
		element_num = -1;
	}
};

template<typename T>
struct UpdateBuffer
{
	void setup(UpdateBufferDescriptor& desc)
	{
		assert(!m_device_buffer.isValid());
		assert(desc.device_memory.isValid());
		assert(btr::isOn(desc.device_memory.getBufferCreateInfo().usage, vk::BufferUsageFlagBits::eTransferDst));

		m_device_buffer = desc.device_memory.allocateMemory(sizeof(T)*desc.element_num);
		m_staging_buffer = desc.staging_memory.allocateMemory(sizeof(T)*desc.element_num*desc.frame_max);
		m_element_max = desc.element_num;
		m_area.resize(desc.frame_max);
	}

	void subupdate(const T* data, vk::DeviceSize data_num, uint32_t offset_num, uint32_t cpu_index)
	{
		auto data_size = sizeof(T)*data_num;
		auto* ptr = m_staging_buffer.getMappedPtr<T>(cpu_index*m_element_max+offset_num);
		memcpy_s(ptr, data_size, data, data_size);

		flushSubBuffer(data_num, offset_num, cpu_index);
	}

	/**
	 * subupdateのstagingバッファを自分で更新する
	 * 使い終わったらflushSubBufferを呼ぶ
	 */
	T* mapSubBuffer(uint32_t cpu_index, uint32_t offset_num = 0)
	{
		return m_staging_buffer.getMappedPtr<T>(cpu_index*m_element_max+ offset_num);
	}

	/**
　	 * 使った部分をマークする
	 */
	void flushSubBuffer(vk::DeviceSize data_num, uint32_t offset_num, uint32_t cpu_index)
	{
		auto data_size = sizeof(T)*data_num;
		auto& area = m_area[cpu_index];
		area.m_begin = std::min(sizeof(T)*offset_num, area.m_begin);
		area.m_end = std::max(sizeof(T)*offset_num + data_size, area.m_end);
	}

	vk::BufferCopy update(uint32_t cpu_index)
	{
		auto& area = m_area[cpu_index];
		if (area.isZero()) {
			// subupdateを読んでいないのでバグの可能性高し
			assert(false);
			return vk::BufferCopy();
		}

		vk::BufferCopy copy_info;
		copy_info.setSize(area.m_end - area.m_begin);
		copy_info.setSrcOffset(m_staging_buffer.getBufferInfo().offset + sizeof(T)*m_element_max*cpu_index + area.m_begin);
		copy_info.setDstOffset(m_device_buffer.getBufferInfo().offset + area.m_begin);

		area.reset();

		return copy_info;
	}

	vk::DescriptorBufferInfo getBufferInfo()const { return m_device_buffer.getBufferInfo(); }
	vk::DescriptorBufferInfo getStagingBufferInfo()const { return m_staging_buffer.getBufferInfo(); }
	BufferMemory& getBufferMemory() { return m_device_buffer; }
	const BufferMemory& getBufferMemory()const { return m_device_buffer; }

private:
	BufferMemory m_device_buffer;
	BufferMemory m_staging_buffer;
	struct Area
	{
		vk::DeviceSize m_begin;
		vk::DeviceSize m_end;

		Area()
		{
			reset();
		}
		void reset()
		{
			m_begin = ~vk::DeviceSize(0);
			m_end = vk::DeviceSize(0);
		}

		bool isZero()const
		{
			return m_end < m_begin;
		}
	};
	std::vector<Area> m_area;
	uint32_t m_element_max;

};


}
