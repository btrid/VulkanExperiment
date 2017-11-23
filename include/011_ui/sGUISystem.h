#pragma once

/*
struct sGUISystem : Singleton<sGUISystem>
{
	friend Singleton<sGUISystem>;

	void setup(const std::shared_ptr<btr::Context>& context);
	std::shared_ptr<UI> create(const std::shared_ptr<btr::Context>& context);

	void addRender(std::shared_ptr<UI>& ui)
	{
		m_render.push_back(ui);
	}
	vk::CommandBuffer draw(const std::shared_ptr<btr::Context>& context);
private:
	enum Shader
	{
		SHADER_ANIMATION,
		SHADER_UPDATE,
		SHADER_TRANSFORM,
		SHADER_VERT_RENDER,
		SHADER_FRAG_RENDER,

		SHADER_NUM,
	};
	enum Pipeline
	{
		PIPELINE_ANIMATION,
		PIPELINE_UPDATE,
		PIPELINE_TRANSFORM,
		PIPELINE_RENDER,
		PIPELINE_NUM,
	};
	enum PipelineLayout
	{
		PIPELINE_LAYOUT_ANIMATION,
		PIPELINE_LAYOUT_UPDATE,
		PIPELINE_LAYOUT_TRANSFORM,
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};

	vk::UniqueDescriptorPool		m_descriptor_pool;
	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout;

	std::array<vk::UniqueShaderModule, SHADER_NUM>				m_shader_module;
	std::array<vk::UniquePipeline, PIPELINE_NUM>				m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;

	btr::BufferMemoryEx<UIGlobal> m_global;
	std::vector<std::shared_ptr<UI>> m_render;

	// 	vk::ImageCreateInfo image_info;
	// 	image_info.imageType = vk::ImageType::e2D;
	// 	image_info.format = vk::Format::eR32Sfloat;
	// 	image_info.mipLevels = 1;
	// 	image_info.arrayLayers = 1;
	// 	image_info.samples = vk::SampleCountFlagBits::e1;
	// 	image_info.tiling = vk::ImageTiling::eLinear;
	// 	image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	// 	image_info.sharingMode = vk::SharingMode::eExclusive;
	// 	image_info.initialLayout = vk::ImageLayout::eUndefined;
	// 	image_info.extent = { 1, 1, 1 };
	// 	image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
	// 	auto image = context->m_device->createImageUnique(image_info);
	// 
	// 	vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(image.get());
	// 	vk::MemoryAllocateInfo memory_alloc_info;
	// 	memory_alloc_info.allocationSize = memory_request.size;
	// 	memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_gpu.getHandle(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

};

*/