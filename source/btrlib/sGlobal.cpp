#include <btrlib/cWindow.h>

#include <filesystem>
#include <vector>
#include <iostream>
#include <fstream>

sGlobal::sGlobal()
	: m_current_frame(0)
	, m_tick_tock(0)
	, m_totaltime(0.f)
{
	m_deltatime = 0.016f;

}


void sGlobal::sync()
{
	m_current_frame = (m_current_frame+1) % FRAME_COUNT_MAX;
	m_tick_tock = (m_tick_tock + 1) % 2;
	auto next = (m_current_frame+1) % FRAME_COUNT_MAX;
	m_deltatime = m_timer.getElapsedTimeAsSeconds();
	m_deltatime = glm::min(m_deltatime, 0.02f);
	m_totaltime += m_deltatime;
}


vk::UniqueShaderModule loadShaderUnique(const vk::Device& device, const std::string& filename)
{
	std::experimental::filesystem::path filepath(filename);
	std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
	if (!file.is_open()) {
		printf("file not found \"%s\"", filename.c_str());
		assert(file.is_open());
	}

	size_t file_size = (size_t)file.tellg();
	file.seekg(0);

	std::vector<char> buffer(file_size);
	file.read(buffer.data(), buffer.size());

	vk::ShaderModuleCreateInfo shaderInfo = vk::ShaderModuleCreateInfo()
		.setPCode(reinterpret_cast<const uint32_t*>(buffer.data()))
		.setCodeSize(buffer.size());
	return device.createShaderModuleUnique(shaderInfo);
}

vk::UniqueDescriptorPool createDescriptorPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings, uint32_t set_size)
{
	std::vector<vk::DescriptorPoolSize> pool_size(VK_DESCRIPTOR_TYPE_RANGE_SIZE);
	for (auto& binding : bindings)
	{
		for (auto& b : binding)
		{
			pool_size[(uint32_t)b.descriptorType].setType(b.descriptorType);
			pool_size[(uint32_t)b.descriptorType].descriptorCount += b.descriptorCount*set_size;
		}
	}
	pool_size.erase(std::remove_if(pool_size.begin(), pool_size.end(), [](auto& p) {return p.descriptorCount == 0; }), pool_size.end());
	vk::DescriptorPoolCreateInfo pool_info;
	pool_info.setPoolSizeCount((uint32_t)pool_size.size());
	pool_info.setPPoolSizes(pool_size.data());
	pool_info.setMaxSets(set_size);
	return device.createDescriptorPoolUnique(pool_info);

}
