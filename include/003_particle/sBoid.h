#pragma once

#include <btrlib/Define.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/Loader.h>

#include <003_particle/CircleIndex.h>
class sBoid : public Singleton<sBoid>
{
	friend Singleton<sBoid>;

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
		glm::vec4 m_inertia;

		glm::ivec2 m_map_index;
		glm::vec2 m_astar_target;

		uint m_brain_index;
		uint m_soldier_type;
		uint m_ll_next;
		float m_life;

	};
public:
	enum : uint32_t
	{
		//		COMPUTE_UPDATE_BRAIN,
		SHADER_COMPUTE_UPDATE_SOLDIER,
		SHADER_COMPUTE_EMIT_SOLDIER,
		SHADER_VERTEX_DRAW_SOLDIER,
		SHADER_FRAGMENT_DRAW_SOLDIER,
		SHADER_NUM,
	};
	enum Pipeline : uint32_t
	{
		PIPELINE_COMPUTE_SOLDIER_UPDATE,
		PIPELINE_COMPUTE_SOLDIER_EMIT,
		PIPELINE_GRAPHICS_SOLDIER_DRAW,
		PIPELINE_NUM,
	};
	enum PipelineLayout : uint32_t
	{
		PIPELINE_LAYOUT_SOLDIER_UPDATE,
		PIPELINE_LAYOUT_SOLDIER_EMIT,
		PIPELINE_LAYOUT_SOLDIER_DRAW,
		PIPELINE_LAYOUT_NUM,
	};
	enum DescriptorSetLayout : uint32_t
	{
		DESCRIPTOR_SET_LAYOUT_SOLDIER_UPDATE,
		DESCRIPTOR_SET_LAYOUT_SOLDIER_EMIT,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet : uint32_t
	{
		DESCRIPTOR_SET_SOLDIER_UPDATE,
		DESCRIPTOR_SET_SOLDIER_EMIT,
		DESCRIPTOR_SET_NUM,
	};
private:
	BoidInfo m_boid_info;
	btr::AllocatedMemory m_boid_info_gpu;
	btr::AllocatedMemory m_soldier_info_gpu;
	DoubleBuffer m_brain_gpu;
	DoubleBuffer m_soldier_gpu;
	btr::UpdateBuffer<std::array<SoldierData, 1024>> m_soldier_emit_gpu;
	btr::AllocatedMemory m_soldier_draw_indiret_gpu;
	DoubleBuffer m_soldier_LL_head_gpu;


	std::array<vk::Pipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::DescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;


	std::vector<vk::Pipeline> m_graphics_pipeline;


	vk::Image m_astar_image;
	vk::ImageView m_astar_image_view;
	vk::DeviceMemory m_astar_image_memory;

	glm::vec3 cell_size;

public:
	void setup(std::shared_ptr<btr::Loader>& loader);
	void execute(std::shared_ptr<btr::Executer>& executer);
	void draw(vk::CommandBuffer cmd);

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_pipeline_layout[layout]; }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor]; }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i]; }
	btr::AllocatedMemory& getSoldier() { m_soldier_gpu.m_buffer; }
	btr::AllocatedMemory& getLL() { return m_soldier_LL_head_gpu.m_buffer; }
};
