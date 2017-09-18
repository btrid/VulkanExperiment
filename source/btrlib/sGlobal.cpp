#include <btrlib/cWindow.h>
#include <btrlib/sValidationLayer.h>

#include <filesystem>
#include <vector>
#include <iostream>
#include <fstream>

namespace btr{
void* VKAPI_PTR Allocation(void* pUserData, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return malloc(size);
}

void* VKAPI_PTR Reallocation(void* pUserData, void* pOriginal, size_t size, size_t alignment, VkSystemAllocationScope allocationScope)
{
	return realloc(pOriginal, size);

}

void VKAPI_PTR Free(void* pUserData, void* pMemory)
{
	free(pMemory);
}

void VKAPI_PTR InternalAllocationNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	printf("alloc\n");
}

void VKAPI_PTR InternalFreeNotification(void* pUserData, size_t size, VkInternalAllocationType allocationType, VkSystemAllocationScope allocationScope)
{
	printf("free\n");
}

}


sGlobal::sGlobal()
	: m_current_frame(0)
	, m_game_frame(0)
	, m_tick_tock(0)
{
	m_deltatime = 0.016f;
	{
		vk::ApplicationInfo appInfo = { "Vulkan Test", 1, "EngineName", 0, VK_API_VERSION_1_0 };
		vk::InstanceCreateInfo instanceInfo = {};
		instanceInfo.setPApplicationInfo(&appInfo);
		instanceInfo.setEnabledExtensionCount((uint32_t)btr::sValidationLayer::Order().getExtensionName().size());
		instanceInfo.setPpEnabledExtensionNames(btr::sValidationLayer::Order().getExtensionName().data());
		instanceInfo.setEnabledLayerCount((uint32_t)btr::sValidationLayer::Order().getLayerName().size());
		instanceInfo.setPpEnabledLayerNames(btr::sValidationLayer::Order().getLayerName().data());
		m_instance = vk::createInstance(instanceInfo);
	}

	{
		auto gpus = m_instance.enumeratePhysicalDevices();
		m_gpu.reserve(gpus.size());
		for (auto&& pd : gpus)
		{
			m_gpu.emplace_back();
			m_gpu.back().setup(pd);
		}
	}

	auto init_thread_data_func = [=](const cThreadPool::InitParam& param)
	{
		SetThreadIdealProcessor(::GetCurrentThread(), param.m_index);
		auto& data = sThreadLocal::Order();
		data.m_thread_index = param.m_index;
	};

	vk::AllocationCallbacks cb;
	cb.setPfnAllocation(btr::Allocation);
	cb.setPfnFree(btr::Free);
	cb.setPfnReallocation(btr::Reallocation);
	cb.setPfnInternalAllocation(btr::InternalAllocationNotification);
	cb.setPfnInternalFree(btr::InternalFreeNotification);

	auto device = m_gpu[0].getDevice();

	m_thread_pool.start(std::thread::hardware_concurrency()-1, init_thread_data_func);
}


void sGlobal::sync()
{
	m_game_frame++;
	m_game_frame = m_game_frame % (std::numeric_limits<decltype(m_game_frame)>::max() / FRAME_MAX*FRAME_MAX);
	m_current_frame = m_game_frame % FRAME_MAX;
	m_tick_tock = (m_tick_tock + 1) % 2;
	auto next = (m_current_frame+1) % FRAME_MAX;
	m_deltatime = m_timer.getElapsedTimeAsSeconds();
	m_deltatime = glm::min(m_deltatime, 0.02f);
}


vk::UniqueShaderModule loadShaderUnique(const vk::Device& device, const std::string& filename)
{
	std::experimental::filesystem::path filepath(filename);
	std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
	if (!file.is_open()) {
		assert(false);
		return vk::UniqueShaderModule();
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
vk::DescriptorPool createPool(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings)
{
	std::vector<vk::DescriptorPoolSize> pool_size(VK_DESCRIPTOR_TYPE_RANGE_SIZE);
	for (auto& binding : bindings)
	{
		for (auto& b : binding)
		{
			pool_size[(uint32_t)b.descriptorType].setType(b.descriptorType);
			pool_size[(uint32_t)b.descriptorType].descriptorCount++;
		}
	}
	pool_size.erase(std::remove_if(pool_size.begin(), pool_size.end(), [](auto& p) {return p.descriptorCount == 0; }), pool_size.end());
	vk::DescriptorPoolCreateInfo pool_info;
	pool_info.setPoolSizeCount((uint32_t)pool_size.size());
	pool_info.setPPoolSizes(pool_size.data());
	pool_info.setMaxSets((uint32_t)bindings.size());
	return device.createDescriptorPool(pool_info);

}

std::unique_ptr<Descriptor> createDescriptor(vk::Device device, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings)
{
	std::unique_ptr<Descriptor> descriptor = std::make_unique<Descriptor>();
	descriptor->m_descriptor_pool = createPool(device, bindings);
	descriptor->m_descriptor_set_layout.resize(bindings.size());
	for (size_t i = 0; i < bindings.size(); i++)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount((uint32_t)bindings[i].size())
			.setPBindings(bindings[i].data());
		descriptor->m_descriptor_set_layout[i] = device.createDescriptorSetLayout(descriptor_set_layout_info);
	}

	vk::DescriptorSetAllocateInfo alloc_info;
	alloc_info.descriptorPool = descriptor->m_descriptor_pool;
	alloc_info.descriptorSetCount = (uint32_t)descriptor->m_descriptor_set_layout.size();
	alloc_info.pSetLayouts = descriptor->m_descriptor_set_layout.data();
	descriptor->m_descriptor_set = device.allocateDescriptorSets(alloc_info);
	return std::move(descriptor);
}

std::unique_ptr<Descriptor> createDescriptor(vk::Device device, vk::DescriptorPool pool, const std::vector<std::vector<vk::DescriptorSetLayoutBinding>>& bindings)
{
	std::unique_ptr<Descriptor> descriptor = std::make_unique<Descriptor>();
	descriptor->m_descriptor_pool = pool;
	descriptor->m_descriptor_set_layout.resize(bindings.size());
	for (size_t i = 0; i < bindings.size(); i++)
	{
		vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
			.setBindingCount((uint32_t)bindings[i].size())
			.setPBindings(bindings[i].data());
		descriptor->m_descriptor_set_layout[i] = device.createDescriptorSetLayout(descriptor_set_layout_info);
	}

	vk::DescriptorSetAllocateInfo alloc_info;
	alloc_info.descriptorPool = descriptor->m_descriptor_pool;
	alloc_info.descriptorSetCount = (uint32_t)descriptor->m_descriptor_set_layout.size();
	alloc_info.pSetLayouts = descriptor->m_descriptor_set_layout.data();
	descriptor->m_descriptor_set = device.allocateDescriptorSets(alloc_info);
	return std::move(descriptor);
}
