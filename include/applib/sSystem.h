#pragma once

#include <memory>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Module.h>
namespace btr
{
	struct Context;
}


struct SystemDescriptor : public UniqueDescriptorModule
{
	SystemDescriptor(const std::shared_ptr<btr::Context>& context);

public:
	struct SystemData
	{
		uint32_t m_gpu_index;
		uint32_t m_gpu_frame;
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
	btr::UpdateBuffer<SystemData> m_data;
};

struct sSystem : public Singleton<sSystem>
{
	friend class Singleton<sSystem>;

	void setup(const std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer execute(const std::shared_ptr<btr::Context>& context);

	std::shared_ptr<SystemDescriptor> m_system_descriptor;
public:
	const SystemDescriptor& getSystemDescriptor()const { return *m_system_descriptor; }

};

