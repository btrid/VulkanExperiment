#pragma once

#include <unordered_map>
#include <mutex>
#include <string>
#include <memory>

template<typename T>
struct ResourceManager
{
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
		auto deleter = [=](T* ptr) { release(ptr->m_filename); delete ptr; };
		resource = std::shared_ptr<T>(new T, deleter);
		resource->m_filename = filename;
		m_resource_list[filename] = resource;
		return false;
	}

private:
// 	std::shared_ptr<T> manage(const std::string& filename)
// 	{
// 		auto deleter = [](T* ptr) { release(ptr->m_filename); delete ptr; };
// 		auto resource = std::shared_ptr<Resource>(new T, deleter);
// 		resource->m_filename = filename;
// 
// 		std::lock_guard<std::mutex> lock(m_mutex);
// 
// 		// 当然まだ登録してないよね？
// 		assert(m_resource_list.find(filename) == m_resource_list.end());
// 
// 	}

	void release(const std::string& filename)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_resource_list.erase(filename);
	}


};
