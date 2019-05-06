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
#include <cstring>
namespace btr {
	std::string getResourceAppPath();
	std::string getResourceLibPath();
	std::string getResourceBtrLibPath();
	std::string getResourceShaderPath();
	void setResourceAppPath(const std::string& str);
	void setResourceLibPath(const std::string& str);
}

#define USE_DEBUG_REPORT 1
struct DebugLabel
{
	DebugLabel(vk::CommandBuffer cmd, vk::DispatchLoaderDynamic& dispatcher, const char* label, const std::array<float, 4>& color)
		: m_cmd(cmd)
		, m_dispatcher(dispatcher)

	{
#if USE_DEBUG_REPORT
		vk::DebugUtilsLabelEXT label_info;
		memcpy_s(label_info.color, sizeof(label_info.color), color.data(), sizeof(label_info.color));
		label_info.pLabelName = label;
		cmd.beginDebugUtilsLabelEXT(label_info, dispatcher);
#endif
	}

	void insert(const char* label, const std::array<float, 4>& color)
	{
#if USE_DEBUG_REPORT
		vk::DebugUtilsLabelEXT label_info;
		memcpy_s(label_info.color, sizeof(label_info.color), color.data(), sizeof(label_info.color));
		label_info.pLabelName = label;
		m_cmd.insertDebugUtilsLabelEXT(label_info, m_dispatcher);
#endif
	}
	~DebugLabel()
	{
#if USE_DEBUG_REPORT
		m_cmd.endDebugUtilsLabelEXT(m_dispatcher);
#endif
	}
	vk::CommandBuffer m_cmd;
	vk::DispatchLoaderDynamic m_dispatcher;

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
