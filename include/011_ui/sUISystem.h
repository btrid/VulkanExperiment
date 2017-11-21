#pragma once

#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

struct UIInfo
{
	uint m_object_num; // Node + sprite + boundary
	uint m_node_num;
	uint m_sprite_num;
	uint m_boundary_num;
};

struct UIParam
{
	vec2 m_position; //!< parentからの位置
	vec2 m_size;
	vec4 m_color;
//	uint32_t m_hash;
	uint32_t _p13;
	uint32_t m_flag;
	int32_t m_parent;
	int32_t m_depth;

	vec2 m_position_anime; //!< animationで移動オフセット
	vec2 m_size_anime;
	vec4 m_color_anime;
	uint32_t m_flag_anime;
	uint32_t _p21;
	uint32_t _p22;
	uint32_t _p23;
};

struct UIObject 
{
	std::string m_name;
	UIParam m_param;

//	std::array<Texture, 64> m_color_image;
};
struct UI
{
	btr::BufferMemoryEx<UIInfo> m_info;
	btr::BufferMemoryEx<UIParam> m_object;

	vk::UniqueImage m_ui_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_ui_texture_memory;
};
struct sUISystem : Singleton<sUISystem>
{
	friend Singleton<sUISystem>;

	void setup(const std::shared_ptr<btr::Context>& context);
	std::shared_ptr<UI> create(const std::shared_ptr<btr::Context>& context);

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
	vk::UniqueDescriptorSet			m_descriptor_set;

	std::array<vk::UniqueShaderModule, SHADER_NUM>				m_shader_module;
	std::array<vk::UniquePipeline, PIPELINE_NUM>				m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;


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

