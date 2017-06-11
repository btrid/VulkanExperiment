#pragma once

#include <btrlib/Define.h>
#include <btrlib/BufferMemory.h>
#include <applib/Loader.h>

#include <003_particle/CircleIndex.h>
struct Boid
{
	struct DoubleBuffer
	{
		btr::AllocatedMemory m_buffer;
		CircleIndex<uint32_t, 2> m_index;

		std::array<vk::DescriptorBufferInfo, 2> m_buffer_info;
		void setup(const btr::AllocatedMemory& buffer)
		{
			m_buffer = buffer;
			m_buffer_info[0] = m_buffer.getBufferInfo();
			m_buffer_info[1] = m_buffer.getBufferInfo();
			m_buffer_info[0].range /= 2;
			m_buffer_info[1].offset += m_buffer_info[0].range;
		}
		void swap()
		{
			m_index.increment();
		}

		vk::DescriptorBufferInfo getOrg()const { return m_buffer.getBufferInfo(); }
		vk::DescriptorBufferInfo getSrc()const { return m_buffer_info[m_index.get()]; }
		vk::DescriptorBufferInfo getDst()const { return m_buffer_info[m_index.getPrev()]; }
		uint32_t getSrcOffset()const { return m_index.get() == 1 ? m_buffer_info[0].range : 0; }
		uint32_t getDstOffset()const { return m_index.get() == 0 ? m_buffer_info[0].range : 0; }
		uint32_t getSrcIndex()const { return m_index.get(); }
		uint32_t getDstIndex()const { return m_index.getPrev(); }

	};
	struct BoidInfo
	{
		uint m_brain_max;
		uint m_soldier_max;
		uint m_soldier_info_max;
	};
	struct BrainData
	{
		uint m_is_alive;
		uint m_type;
		uint _p[2];
		glm::vec4 m_target;
	};
	struct SoldierInfo
	{
		float m_move_speed;
		float m_turn_speed;
		uint _p2;
		uint _p3;
	};

	struct SoldierData
	{
		glm::vec4 m_pos;	//!< xyz:pos w:scale
		glm::vec4 m_vel;	//!< xyz:dir w:distance
		glm::ivec4 m_map_index;
		uint m_brain_index;
		uint m_soldier_type;
		uint m_ll_next;
		float m_life;
		glm::vec4 m_astar_target;
	};

	enum : uint32_t
	{
		//		COMPUTE_UPDATE_BRAIN,
		UPDATE_SOLDER_CS,
		EMIT_SOLDER_CS,
		DRAW_SOLDER_VS,
		DRAW_SOLDER_FS,
		SHADER_NUM,
	};
	enum : uint32_t
	{
		PIPELINE_LAYOUT_UPDATE,
		PIPELINE_LAYOUT_EMIT,
		PIPELINE_LAYOUT_SOLDIER_DRAW,
		PIPELINE_LAYOUT_NUM,
	};
	enum : uint32_t
	{
		DESCRIPTOR_UPDATE,
		DESCRIPTOR_EMIT,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};

	BoidInfo m_boid_info;
	btr::AllocatedMemory m_boid_info_gpu;
	btr::AllocatedMemory m_soldier_info_gpu;
	DoubleBuffer m_brain_gpu;
	DoubleBuffer m_soldier_gpu;
	btr::UpdateBuffer<std::array<SoldierData, 1024>> m_soldier_emit_gpu;
	btr::AllocatedMemory m_soldier_draw_indiret_gpu;
	DoubleBuffer m_soldier_LL_head_gpu;


	std::unique_ptr<Descriptor> m_descriptor;

	vk::PipelineCache m_cache;
	std::vector<vk::Pipeline> m_compute_pipeline;
	std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	std::vector<vk::Pipeline> m_graphics_pipeline;


	vk::Image m_astar_image;
	vk::ImageView m_astar_image_view;
	vk::DeviceMemory m_astar_image_memory;

	struct cParticlePipeline* m_parent;
	glm::vec3 cell_size;
	void setup(app::Loader& loader, struct cParticlePipeline& parent);
	void execute(vk::CommandBuffer cmd);
	void draw(vk::CommandBuffer cmd);

};
