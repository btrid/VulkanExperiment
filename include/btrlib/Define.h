#pragma once

#define BTR_USE_REVERSED_Z 1
#define NOMINMAX
#include <Windows.h>
#define VK_USE_PLATFORM_WIN32_KHR
//#define VULKAN_HPP_NO_SMART_HANDLE
#ifdef MemoryBarrier
#undef MemoryBarrier
#endif // MemoryBarrier
// #define VULKAN_HPP_NO_EXCEPTIONS
// #define VULKAN_HPP_DISABLE_ENHANCED_MODE
#include <vulkan/vulkan.hpp>

#include <btrlib/DefineMath.h>

#include <string>
namespace btr {
	std::string getResourceAppPath();
	std::string getResourceLibPath();
	std::string getResourceBtrLibPath();
	std::string getResourceShaderPath();
	void setResourceAppPath(const std::string& str);
	void setResourceLibPath(const std::string& str);
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

#define array_length(_a) ((uint32_t)(sizeof(_a) / sizeof(_a[0])))
