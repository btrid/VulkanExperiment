#pragma once

#include <unordered_map>
#include <mutex>
#include <string>
#include <memory>
#include <btrlib/sGlobal.h>

template<typename T>
struct ResourceManager
{
	struct ManageHandle : public T
	{
		std::string _m_hash;
		uint m_block;
	};
	std::unordered_map<std::string, std::weak_ptr<T>> m_resource_list;
	std::vector<uint32_t> m_active;
	std::vector<uint32_t> m_free;
	uint32_t m_consume;
	uint32_t m_accume;
	std::mutex m_mutex;

	ResourceManager()
	{
		m_consume = 0;
		m_accume = 0;
		m_active.resize(1024);
		for (size_t i = 0; i < m_active.size(); i++)
		{
			m_active[i] = i;
		}
	}
	/**
	 * @brief	リソースの管理を行う
	 *			@return true	すでに管理されている
						false	新しいリソースを生成した
	 */
	bool manage(std::shared_ptr<T>& resource, const std::string& filename)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_resource_list.find(filename);
		if (it != m_resource_list.end()) {
			resource = it->second.lock();
			return true;
		}
		auto deleter = [&](ManageHandle* ptr) { release(ptr); sDeleter::Order().enque(std::unique_ptr<ManageHandle>(ptr)); };
		auto _r = std::shared_ptr<ManageHandle>(new ManageHandle, deleter);
		resource = _r;
		_r->_m_hash = filename;
		_r->m_block = m_active[m_consume];

		m_resource_list[filename] = resource;
		m_consume = (m_consume + 1) % m_active.size();
		return false;
	}

private:

	void release(ManageHandle* p)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_resource_list.erase(p->_m_hash);
		m_active[m_accume] = p->m_block;
		m_accume = (m_accume + 1) % m_active.size();
	}


};
