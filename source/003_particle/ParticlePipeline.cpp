#include <003_particle/ParticlePipeline.h>

vk::DescriptorPool createPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings)
{
	std::vector<vk::DescriptorPoolSize> pool_size(VK_DESCRIPTOR_TYPE_RANGE_SIZE);
	for (auto& binding: bindings)
	{
		for (auto& b : binding)
		{
			pool_size[(uint32_t)b.descriptorType].setType(b.descriptorType);
			pool_size[(uint32_t)b.descriptorType].descriptorCount++;
		}
	}
	pool_size.erase(std::remove_if(pool_size.begin(), pool_size.end(), [](auto& p) {return p.descriptorCount == 0; }), pool_size.end());
	vk::DescriptorPoolCreateInfo pool_info;
	pool_info.setPoolSizeCount(pool_size.size());
	pool_info.setPPoolSizes(pool_size.data());
	//				pool_info.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
	pool_info.setMaxSets(bindings.size());
	return device.createDescriptorPool(pool_info);

}
