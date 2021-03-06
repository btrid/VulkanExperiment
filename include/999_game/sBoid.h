#pragma once

#include <btrlib/Define.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <applib/Utility.h>
#include <applib/App.h>

class sBoid : public Singleton<sBoid>
{
	friend Singleton<sBoid>;

	struct DoubleBuffer
	{
		btr::BufferMemory m_buffer;

		std::array<vk::DescriptorBufferInfo, 2> m_buffer_info;
		void setup(const btr::BufferMemory& buffer)
		{
			m_buffer = buffer;
			m_buffer_info[0] = m_buffer.getInfo();
			m_buffer_info[1] = m_buffer.getInfo();
			m_buffer_info[0].range /= 2;
			m_buffer_info[1].range /= 2;
			m_buffer_info[1].offset += m_buffer_info[0].range;
		}

		vk::DescriptorBufferInfo getOrg()const { return m_buffer.getInfo(); }
		vk::DescriptorBufferInfo getInfo(uint32_t index)const { return m_buffer_info[index]; }

	};
	struct BoidInfo
	{
		uint m_brain_max;
		uint m_soldier_max;
		uint m_soldier_info_max;
		uint m_soldier_emit_max;

		BoidInfo()
		{
			m_brain_max = 256;
			m_soldier_max = 8192/2;
			m_soldier_info_max = 16;
			m_soldier_emit_max = 1024;

		}
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

		uint m_ll_next;
		uint m_brain_index;
		uint m_soldier_type;
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
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet : uint32_t
	{
		DESCRIPTOR_SET_SOLDIER_UPDATE,
		DESCRIPTOR_SET_NUM,
	};
private:
	btr::BufferMemoryEx<BoidInfo> m_boid_info_gpu;
	btr::BufferMemoryEx<SoldierInfo> m_soldier_info_gpu;
	DoubleBuffer m_brain_gpu;
	DoubleBuffer m_soldier_gpu;
	btr::BufferMemoryEx<SoldierData> m_soldier_emit_data;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> m_soldier_draw_indiret_gpu;
	DoubleBuffer m_soldier_LL_head_gpu;

	BoidInfo m_boid_info;
	std::array<AppendBuffer<SoldierData, 1024>, 2> m_emit_data_cpu;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;
	std::shared_ptr<RenderTarget> m_render_target;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;

	vk::UniqueImage m_astar_image;
	vk::UniqueImageView m_astar_image_view;
	vk::UniqueDeviceMemory m_astar_image_memory;

	glm::vec3 cell_size;

public:
	void setup(std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target);
	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_pipeline_layout[layout].get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }
	btr::BufferMemory& getSoldier() { return m_soldier_gpu.m_buffer; }
	btr::BufferMemory& getLL() { return m_soldier_LL_head_gpu.m_buffer; }
};
