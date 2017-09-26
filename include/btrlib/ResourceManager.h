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
	};
	std::unordered_map<std::string, std::weak_ptr<T>> m_resource_list;
	std::mutex m_mutex;

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
		auto deleter = [&](T* ptr) { release(ptr->m_filename); sDeleter::Order().enque(std::unique_ptr<T>(ptr)); };
		resource = std::shared_ptr<T>(new T, deleter);
		resource->m_filename = filename;
		m_resource_list[filename] = resource;
		return false;
	}

private:

	void release(const std::string& filename)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_resource_list.erase(filename);
	}


};
