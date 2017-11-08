#pragma once
#include <memory>
#include <array>
#include <btrlib/Singleton.h>
#include <btrlib/Context.h>

struct TileInfo
{
	uvec2 m_resolusion;
	uvec2 m_tile_num;
	uint m_tile_index_map_max;
	uint m_tile_buffer_max_num;
};
struct LightInfo
{
	uint m_max_num;
};

enum class SpotLightType
{
	Effect,	
};
struct SpotLightInfo
{
	float m_cone_angle; // コーンアングルの角度
	float m_penumbra_angle; //… 有効拡散角度。ライトのエッジがボケる大きさを設定します
	float m_dropoff; //… スポットライトのビームの中心からエッジに向かって、ライトの明るさが減衰する割合を設定します
};
struct LightData
{
	vec4 m_pos;	//!< xyz:pos w:scale
	vec4 m_normal;
	vec4 m_emissive;
};
struct sLightSystem : public Singleton<sLightSystem>
{
	friend Singleton<sLightSystem>;

	enum : uint32_t {
		SHADER_PARTICLE_COLLECT,
		SHADER_BULLET_COLLECT,
		SHADER_EFFECT_COLLECT,
		SHADER_TILE_CULLING,
		SHADER_NUM,
	};

	enum : uint32_t
	{
		PIPELINE_LAYOUT_PARTICLE_COLLECT,
		PIPELINE_LAYOUT_BULLET_COLLECT,
		PIPELINE_LAYOUT_EFFECT_COLLECT,
		PIPELINE_LAYOUT_TILE_CULLING,
		PIPELINE_LAYOUT_NUM,
	};
	enum : uint32_t {
		PIPELINE_PARTICLE_COLLECT,
		PIPELINE_BULLET_COLLECT,
		PIPELINE_EFFECT_COLLECT,
		PIPELINE_TILE_CULLING,
		PIPELINE_NUM,
	};

	enum DescriptorSetLayout : uint32_t
	{
		DESCRIPTOR_SET_LAYOUT_LIGHT,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet: uint32_t
	{
		DESCRIPTOR_SET_LIGHT,
		DESCRIPTOR_SET_NUM,
	};

	TileInfo m_tile_info_cpu;

	btr::BufferMemoryEx<LightInfo> m_light_info;
	btr::BufferMemoryEx<TileInfo> m_tile_info;
	btr::BufferMemoryEx<LightData> m_light_data;
	btr::BufferMemoryEx<uint32_t> m_light_data_counter;
	btr::BufferMemoryEx<uint32_t> m_tile_data_counter;
	btr::BufferMemoryEx<uint32_t> m_tile_data_map;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::UniqueDescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;

	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;

	void setup(std::shared_ptr<btr::Context>& context);
	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context);

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor].get(); }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i].get(); }
};