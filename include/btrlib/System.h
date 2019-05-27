#pragma once

#include <array>
#include <memory>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Module.h>
namespace btr
{

struct SystemData
{
	uint32_t m_render_index;
	uint32_t m_render_frame;
	float m_deltatime;
	uint32_t _p13;

	uvec2 m_resolution;
	ivec2 m_mouse_position;

	ivec2 m_mouse_position_old;
	uint32_t m_is_mouse_on;
	uint32_t m_is_mouse_off;

	uint32_t m_is_mouse_hold;
	float m_touch_time;
	uint32_t _p22;
	uint32_t _p23;

	uint32_t m_is_key_on;
	uint32_t m_is_key_off;
	uint32_t m_is_key_hold;
	uint32_t _p33;
};

struct System
{
	System(const std::shared_ptr<btr::Context>& context);
	void execute(vk::CommandBuffer cmd);

	vk::UniqueDescriptorSetLayout m_descriptor_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	btr::BufferMemoryEx<SystemData> m_buffer;
	std::array<BufferMemoryEx<SystemData>, sGlobal::FRAME_COUNT_MAX> m_staging;
public:
	vk::DescriptorSet getSystemDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getSystemDescriptorLayout()const { return m_descriptor_layout.get(); }


	std::shared_ptr<btr::Context> m_context;
};

}
