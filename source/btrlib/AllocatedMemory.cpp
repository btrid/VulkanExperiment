#include <btrlib/AllocatedMemory.h>

void btr::GPUMemoryAllocater::setup(vk::DeviceSize size, vk::DeviceSize align)
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

btr::Zone btr::GPUMemoryAllocater::alloc(vk::DeviceSize size, bool is_reverse)
{
	assert(size != 0);
	if (sGlobal::Order().isElapsed(m_last_gc, 1u))
	{
		gc_impl();
	}

	if (is_reverse)
	{
		return allocReverse(size);
	}

	size = btr::align(size, m_align);
	std::lock_guard<std::mutex> lock(m_free_zone_mutex);
	for (auto it = m_free_zone.begin(); it != m_free_zone.end(); it++)
	{
		if (it->range() == size)
		{
			auto zone = *it;
			m_free_zone.erase(it);
			m_active_zone.emplace_back(zone);
			return m_active_zone.back();
		}
		else if (it->range() > size)
		{
			m_active_zone.emplace_back(it->split(size));
			assert(m_active_zone.back().range() != 0);
			return m_active_zone.back();
		}
	}
	return Zone();
}

void btr::GPUMemoryAllocater::delayedFree(Zone zone)
{
	assert(zone.isValid());
	assert(zone.range() != 0);
	zone.m_wait_frame = 5;

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

void btr::GPUMemoryAllocater::free_impl(const Zone& zone)
{
	assert(zone.isValid());
	assert(zone.range() != 0);
	{
		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
		// activeから削除
		auto it = std::find_if(m_active_zone.begin(), m_active_zone.end(), [&](Zone& active) { return active.m_start == zone.m_start; });
		if (it != m_active_zone.end()) {
			m_active_zone.erase(it);
		}
	}

	{
		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
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

void btr::GPUMemoryAllocater::gc_impl()
{
	{
		auto& list = m_delayed_active;
		std::lock_guard<std::mutex> lock(list.m_mutex);
		for (auto it = list.m_list.begin(); it != list.m_list.end();)
		{
			if (--it->m_wait_frame == 0)
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

	{
		std::lock_guard<std::mutex> lock(m_free_zone_mutex);
		std::sort(m_free_zone.begin(), m_free_zone.end(), [](Zone& a, Zone& b) { return a.m_start < b.m_start; });
		for (size_t i = m_free_zone.size() - 1; i > 0; i--)
		{
			if (m_free_zone[i - 1].tryMarge(m_free_zone[i])) {
				m_free_zone.erase(m_free_zone.begin() + i);
			}
		}

	}
	m_last_gc = sGlobal::Order().getGameFrame();
}

btr::Zone btr::GPUMemoryAllocater::allocReverse(vk::DeviceSize size)
{
	assert(size != 0);
	size = btr::align(size, m_align);
	std::lock_guard<std::mutex> lock(m_free_zone_mutex);
	for (auto it = m_free_zone.rbegin(); it != m_free_zone.rend(); it++)
	{
		if (it->range() == size)
		{
			auto zone = *it;
			m_free_zone.erase(std::next(it).base());
			m_active_zone.emplace_back(zone);
			return m_active_zone.back();
		}
		else if (it->range() > size)
		{
			m_active_zone.emplace_back(it->splitReverse(size));
			assert(m_active_zone.back().range() != 0);
			return m_active_zone.back();
		}
	}
	return Zone();
}
