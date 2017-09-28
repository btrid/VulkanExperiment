#pragma once

#include <btrlib/Define.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>

#include <999_game/CircleIndex.h>
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
			m_buffer_info[0] = m_buffer.getBufferInfo();
			m_buffer_info[1] = m_buffer.getBufferInfo();
			m_buffer_info[0].range /= 2;
			m_buffer_info[1].offset += m_buffer_info[0].range;
		}
		void swap()
		{
//			m_index.increment();
		}

		vk::DescriptorBufferInfo getOrg()const { return m_buffer.getBufferInfo(); }
		vk::DescriptorBufferInfo getSrc()const { return m_buffer_info[getSrcIndex()]; }
		vk::DescriptorBufferInfo getDst()const { return m_buffer_info[getDstIndex()]; }
		uint32_t getSrcOffset()const { return getSrcIndex() == 1 ? m_buffer_info[0].range : 0; }
		uint32_t getDstOffset()const { return getSrcIndex() == 0 ? m_buffer_info[0].range : 0; }
// 		uint32_t getSrcIndex()const { return m_index.get(); }
// 		uint32_t getDstIndex()const { return m_index.getPrev(); }
		uint32_t getSrcIndex()const { return sGlobal::Order().getCPUIndex(); }
		uint32_t getDstIndex()const { return sGlobal::Order().getGPUIndex(); }

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
			m_soldier_max = 8192;
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

	struct EmitData
	{
		struct EmitBuffer
		{
			bool m_is_emit;
			uint32_t m_emit_num;
			btr::BufferMemory m_buffer;
		};
		std::vector<EmitBuffer> m_staging;
		btr::BufferMemory m_buffer;
	};
private:
	BoidInfo m_boid_info;
	btr::BufferMemory m_boid_info_gpu;
	btr::BufferMemory m_soldier_info_gpu;
	DoubleBuffer m_brain_gpu;
	DoubleBuffer m_soldier_gpu;
	EmitData m_emit_data;
	btr::BufferMemory m_soldier_draw_indiret_gpu;
	DoubleBuffer m_soldier_LL_head_gpu;

	vk::UniqueRenderPass m_render_pass;
	std::vector<vk::UniqueFramebuffer> m_framebuffer;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	vk::UniqueImage m_astar_image;
	vk::UniqueImageView m_astar_image_view;
	vk::UniqueDeviceMemory m_astar_image_memory;

	glm::vec3 cell_size;

public:
	void setup(std::shared_ptr<btr::Context>& loader);
	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& executer);
	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& executer);

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_pipeline_layout[layout].get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }
	btr::BufferMemory& getSoldier() { return m_soldier_gpu.m_buffer; }
	btr::BufferMemory& getLL() { return m_soldier_LL_head_gpu.m_buffer; }
};
