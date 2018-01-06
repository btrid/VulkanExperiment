#pragma once

#include <vector>
#include <array>
#include <memory>
#include <btrlib/Singleton.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCamera.h>
#include <btrlib/Context.h>

#include <applib/App.h>
#include <applib/sSystem.h>
#include <applib/Utility.h>
#include <999_game/MazeGenerator.h>
#include <999_game/sScene.h>


struct BulletInfo
{
	uint32_t m_max_num;
};

struct BulletData
{
	glm::vec4 m_pos;	//!< xyz:pos w:scale
	glm::vec4 m_vel;	//!< xyz:dir

	uint32_t m_ll_next;
	float m_power;
	glm::ivec2 m_map_index;

	uint32_t m_type;
	uint32_t m_flag;
	float m_life;
	float m_atk;

	BulletData()
	{
		memset(this, 0, sizeof(BulletData));
// 		m_ll_next = -1;
// 		m_type = -1;
		m_life = -1.f;
//		m_atk = -1.f;
	}
};


class sBulletSystem : public Singleton<sBulletSystem>
{
	friend Singleton<sBulletSystem>;
public:
	enum : uint32_t {
		PIPELINE_COMPUTE_UPDATE,
		PIPELINE_COMPUTE_EMIT,
		PIPELINE_GRAPHICS_RENDER,
		PIPELINE_NUM
	};
	enum : uint32_t {
		SHADER_COMPUTE_UPDATE,
		SHADER_COMPUTE_EMIT,
		SHADER_VERTEX_PARTICLE,
		SHADER_FRAGMENT_PARTICLE,
		SHADER_NUM,
	};

	enum PipelineLayout : uint32_t
	{
		PIPELINE_LAYOUT_UPDATE,
		PIPELINE_LAYOUT_DRAW,
		PIPELINE_LAYOUT_NUM,
	};
	enum DescriptorSetLayout : uint32_t
	{
		DESCRIPTOR_SET_LAYOUT_UPDATE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet : uint32_t
	{
		DESCRIPTOR_SET_UPDATE,
		DESCRIPTOR_SET_NUM,
	};

	btr::BufferMemory m_bullet;
	btr::BufferMemory m_bullet_info;
	std::array<AppendBuffer<BulletData, 1024>, 2> m_data;
	btr::BufferMemory m_bullet_counter;
	btr::BufferMemory m_bullet_emit;
	btr::BufferMemory m_bullet_emit_count;
	btr::BufferMemory m_bullet_draw_indiret_info;
	btr::BufferMemory m_bullet_LL_head_gpu;

	std::shared_ptr<RenderBackbufferModule> m_render_pass;

	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	BulletInfo m_bullet_info_cpu;
	void setup(std::shared_ptr<btr::Context>& context);

	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context);

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context);

public:

	void shoot(const std::vector<BulletData>& param)
	{
		auto& data = m_data[sGlobal::Order().getWorkerIndex()];
		data.push(param.data(), param.size());
	}

	vk::PipelineLayout getPipelineLayout(PipelineLayout layout)const { return m_pipeline_layout[layout].get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }
	btr::BufferMemory& getLL() { return m_bullet_LL_head_gpu; }
	btr::BufferMemory& getBullet() { return m_bullet; }

};

