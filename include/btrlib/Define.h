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

//#define VULKAN_HPP_STORAGE_SHARED
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.hpp>


#include <btrlib/DefineMath.h>

#include <cstdlib>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>


namespace btr {
	std::string getResourceAppPath();
	std::string getResourceLibPath();
	std::string getResourceBtrLibPath();
	std::string getResourceShaderPath();
	void setResourceAppPath(const std::string& str);
	void setResourceLibPath(const std::string& str);
}

#if _DEBUG
#define USE_DEBUG_REPORT 1
#else
#define USE_DEBUG_REPORT 0
#endif
struct DebugLabel
{
	static const uint32_t k_color_default = uint32_t(0xffffffffu);
	static const uint32_t k_color_debug = uint32_t(0x000000ffu);

	DebugLabel(vk::CommandBuffer cmd, const char* label, uint32_t color = k_color_default)
		: m_cmd(cmd)

	{
#if USE_DEBUG_REPORT
		vec4 color_f4 = vec4((uvec4(color) >> uvec4(24, 16, 8, 0)) & uvec4(0xff)) / vec4(255.f);

		vk::DebugUtilsLabelEXT label_info;
		memcpy_s(label_info.color, sizeof(label_info.color), &color_f4, sizeof(color_f4));
		label_info.pLabelName = label;
		cmd.beginDebugUtilsLabelEXT(label_info);
#endif
	}

	void insert(const char* label, uint32_t color = k_color_default)
	{
#if USE_DEBUG_REPORT
		vec4 color_f4 = vec4((uvec4(color) >> uvec4(24, 16, 8, 0)) & uvec4(0xff)) / vec4(255.f);

		vk::DebugUtilsLabelEXT label_info;
		memcpy_s(label_info.color, sizeof(label_info.color), &color_f4, sizeof(color_f4));
		label_info.pLabelName = label;
		m_cmd.insertDebugUtilsLabelEXT(label_info);
#endif
	}
	~DebugLabel()
	{
#if USE_DEBUG_REPORT
		m_cmd.endDebugUtilsLabelEXT();
#endif
	}
	vk::CommandBuffer m_cmd;

};


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

#define array_length(_a) ((uint32_t)std::size(_a))
#define array_size(_a) ((uint32_t)std::size(_a))

namespace Helper
{
uint32_t getMemoryTypeIndex(const vk::PhysicalDevice& gpu, const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag);
}