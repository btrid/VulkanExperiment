#pragma once

#include <vector>
#include <btrlib/Define.h>
/// Logical Device
class cDevice
{
	friend class cGPU;
public:
	cDevice() {}
	~cDevice(){}

	vk::Device*			operator->() { return &m_handle; }
	const vk::Device*	operator->()const { return &m_handle; }


	const std::vector<uint32_t>& getQueueFamilyIndex()const { return m_family_index; }
	uint32_t getQueueFamilyIndex(vk::QueueFlags reqest_queue)const
	{
		//		std::vector<uint32_t> index;
		auto queueProp = m_gpu.getQueueFamilyProperties();
		for (size_t i = 0; i < queueProp.size(); i++) {
			if (btr::isOn(queueProp[i].queueFlags, reqest_queue))
			{
				if (std::find(m_family_index.begin(), m_family_index.end(), i) != m_family_index.end())
				{
					return (uint32_t)i;
				}
			}
		}
		return (uint32_t)-1;
	}
	uint32_t getQueueNum(uint32_t family_index)const { return (uint32_t)m_queue_priority[family_index].size(); }
	uint32_t getQueueNum(vk::QueueFlags reqest_queue)const
	{
		return (uint32_t)m_queue_priority[getQueueFamilyIndex(reqest_queue)].size();
	}
	const vk::Device& getHandle()const { return m_handle; }
	const vk::Device& operator*()const { return m_handle; }
	const vk::PhysicalDevice& getGPU()const { return m_gpu; }

	void DebugMarkerSetObjectTag(vk::DebugMarkerObjectTagInfoEXT* pTagInfo)const
	{
		m_vk_debug_marker_set_object_tag(m_handle, (VkDebugMarkerObjectTagInfoEXT*)pTagInfo);
	}
	void DebugMarkerSetObjectName(vk::DebugMarkerObjectNameInfoEXT* pTagInfo)const
	{
		m_vk_debug_marker_set_object_name(m_handle, (VkDebugMarkerObjectNameInfoEXT*)pTagInfo);
	}
	void CmdDebugMarkerBegin(vk::CommandBuffer cmd, vk::DebugMarkerMarkerInfoEXT* pTagInfo)const
	{
		m_vk_cmd_debug_marker_begin(cmd, (VkDebugMarkerMarkerInfoEXT*)pTagInfo);
	}
	void CmdDebugMarkerEnd(vk::CommandBuffer cmd)const
	{
		m_vk_cmd_debug_marker_end(cmd);
	}
	void CmdDebugMarkerInsert(vk::CommandBuffer cmd, vk::DebugMarkerMarkerInfoEXT* pTagInfo)const
	{
		m_vk_cmd_debug_marker_insert(cmd, (VkDebugMarkerMarkerInfoEXT*)pTagInfo);
	}
private:
	vk::PhysicalDevice m_gpu;
	vk::Device m_handle;
	std::vector<uint32_t> m_family_index;
	std::vector<std::vector<float>> m_queue_priority;
	PFN_vkDebugMarkerSetObjectTagEXT m_vk_debug_marker_set_object_tag;
	PFN_vkDebugMarkerSetObjectNameEXT m_vk_debug_marker_set_object_name;
	PFN_vkCmdDebugMarkerBeginEXT m_vk_cmd_debug_marker_begin;
	PFN_vkCmdDebugMarkerEndEXT m_vk_cmd_debug_marker_end;
	PFN_vkCmdDebugMarkerInsertEXT m_vk_cmd_debug_marker_insert;

};

class cGPU
{

public:
	cGPU() = default;
	~cGPU() = default;
	void setup(vk::PhysicalDevice pd);
	friend class sGlobal;
public:
	vk::PhysicalDevice*			operator->() { return &m_handle; }
	const vk::PhysicalDevice*	operator->()const { return &m_handle; }
	const vk::PhysicalDevice&	getHandle()const { return m_handle; }

	std::vector<uint32_t> getQueueFamilyIndexList(vk::QueueFlags flag)const;
	std::vector<uint32_t> getQueueFamilyIndexList(vk::QueueFlags flag, const std::vector<uint32_t>& useIndex);
	int getMemoryTypeIndex(const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag)const;
	vk::Format getSupportedDepthFormat(const std::vector<vk::Format>& depthFormat)const;

	cDevice& getDevice() { return m_device; }
	const cDevice& getDevice()const { return m_device; }
private:

	vk::PhysicalDevice m_handle;
	cDevice m_device;
public:
	struct Helper
	{
		static uint32_t getMemoryTypeIndex(const vk::PhysicalDevice& gpu, const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag)
		{
			auto& prop = gpu.getMemoryProperties();
			auto memory_type_bits = request.memoryTypeBits;
			for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
			{
				auto bit = memory_type_bits >> i;
				if ((bit & 1) == 0) {
					continue;
				}

				if ((prop.memoryTypes[i].propertyFlags & flag) == flag)
				{
					return i;
				}
			}
			return 0xffffffffu;
		}
	};
};
