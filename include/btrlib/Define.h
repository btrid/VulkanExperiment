#pragma once

#define NOMINMAX
#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
//#define VULKAN_HPP_NO_SMART_HANDLE
#include <vulkan/vulkan.hpp>

#include <btrlib/DefineMath.h>

#include <string>
namespace btr {
	std::string getResourcePath();
	void setResourcePath(const std::string& str);
}

template<typename T>
size_t vector_sizeof(const T& v) {
	return sizeof(T::value_type) * v.size();
}

template<typename T>
size_t alignment(T size, T align)
{
	return (size % align == 0) ? size : (align - (size % align) + size);
}

// template <typename T, size_t SIZE>
// size_t array_length(const T(&array)[SIZE]) {
// 	return SIZE;
// }

#define array_length(array) (sizeof(array) / sizeof(array[0]))