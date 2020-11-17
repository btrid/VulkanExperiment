#include <btrlib/Define.h>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace btr
{
	static std::string s_app_path = "..\\..\\resource\\";
	static std::string s_lib_path = "..\\..\\resource\\applib\\";
	static std::string s_btrlib_path = "..\\..\\resource\\btrlib\\";
	std::string getResourceAppPath()
	{
		return s_app_path;
	}
	void setResourceAppPath(const std::string& str)
	{
		s_app_path = str;
	}
	std::string getResourceLibPath()
	{
		return s_lib_path;
	}
	std::string getResourceShaderPath()
	{
		return "../../resource/shader/binary/";
	}
	void setResourceLibPath(const std::string& str)
	{
		s_lib_path = str;
	}
	std::string getResourceBtrLibPath()
	{
		return s_btrlib_path;
	}

}

namespace Helper
{
	uint32_t getMemoryTypeIndex(const vk::PhysicalDevice& gpu, const vk::MemoryRequirements& request, vk::MemoryPropertyFlags flag)
	{
		auto prop = gpu.getMemoryProperties();
		auto memory_type_bits = request.memoryTypeBits;
		for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
		{
			auto bit = memory_type_bits >> i;
			if ((bit & 1) == 0) {
				continue;
			}

			if ((prop.memoryTypes[i].propertyFlags & flag) != flag)
			{
				continue;
			}
			return i;
		}
		assert(false);
		return 0xffffffffu;
	}

}