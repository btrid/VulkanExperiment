#pragma once

#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

struct UIGlobal
{
	uvec2 m_resolusion; // 解像度
};
struct UIInfo
{
	uint m_object_num; // node + sprite + boundary
	uint m_node_num;
	uint m_sprite_num;
	uint m_boundary_num;
};

enum UIFlagBit
{
	is_use = 1 << 0,
	is_enable = 1 << 1,
	is_visible = 1 << 2,
	_reserved1 = 1 << 3,
	_reserved2 = 1 << 4,
};
struct UIParam
{
	vec2 m_position_local; //!< 自分の場所
	vec2 m_size_local;
	vec4 m_color_local;
	uint32_t m_name_hash;
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

};
struct UI
{
	std::string m_name;
	vk::UniqueDescriptorSet	m_descriptor_set;
	btr::BufferMemoryEx<UIInfo> m_info;
	btr::BufferMemoryEx<UIParam> m_object;

	vk::UniqueImage m_ui_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_ui_texture_memory;

//	std::array<Texture, 64> m_color_image;
};

struct UIManipulater 
{
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<UI> m_ui;
	UIManipulater(const std::shared_ptr<btr::Context>& context, std::shared_ptr<UI>& ui)
	{
		m_context = context;
		m_ui = ui;
	}
	void sort()
	{
//		m_context->m_staging_memory.allocateMemory();
//		m_ui->m_info.getMappedPtr()
	}

	void test();
	struct Cmd 
	{
		void undo(){}
		void redo(){}
	};
};

struct UIAnimationInfo
{

};

struct UIAnimationParam
{

};
struct sUISystem : Singleton<sUISystem>
{
	friend Singleton<sUISystem>;

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

	std::shared_ptr<RenderPassModule> m_render_pass;
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

