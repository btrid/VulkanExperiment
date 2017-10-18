#pragma once

#include <array>
#include <memory>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cCamera.h>
#include <btrlib/Context.h>
#include <btrlib/Singleton.h>
#include <applib/sCameraManager.h>
#include <applib/sSystem.h>

struct MovableInfo
{
	uint32_t m_data_size; // doublebufferのシングルバッファ分
	uint32_t m_active_counter;
};
struct MovableData
{
	uint64_t packed_pos;
	float scale;
	uint32_t type;
};

struct TileInfo
{
	uvec2 m_tile_num;
	uvec2 m_resolusion;
	uint32_t m_counter_max;
};

struct cMovable
{
	btr::BufferMemoryEx<MovableInfo> m_info;
	btr::BufferMemoryEx<MovableData> m_data;
	btr::BufferMemoryEx<TileInfo> m_tile_info;
	btr::BufferMemoryEx<uint32_t> m_tile_counter;
	btr::BufferMemoryEx<uint32_t> m_tile_index;

	void updateDescriptor(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set)
	{
		vk::DescriptorBufferInfo storages[] =
		{
			m_info.getBufferInfo(),
			m_data.getBufferInfo(),
			m_tile_info.getInfo(),
			m_tile_counter.getInfo(),
			m_tile_index.getInfo(),
		};

		vk::WriteDescriptorSet write_desc[] =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(array_length(storages))
			.setPBufferInfo(storages)
			.setDstBinding(0)
			.setDstSet(descriptor_set),
		};
		context->m_device->updateDescriptorSets(array_length(write_desc), write_desc, 0, nullptr);
	}
	static LayoutDescriptor GetLayout()
	{
		LayoutDescriptor desc;
		auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
		desc.binding =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(1),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(2),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(3),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorType(vk::DescriptorType::eStorageBuffer)
			.setDescriptorCount(1)
			.setBinding(4),
		};
		desc.m_set_num = 1;
		return desc;
	}
};

struct Movable
{
	virtual ~Movable() = default;
};
struct sMovable
{
	enum Shader
	{
		SHADER_COMP_TILE_CULLING,
		SHADER_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_CULLING,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_COMPUTE_CULLING,
		PIPELINE_NUM,
	};

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_shader_info;
	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;


	MovableInfo m_info_cpu;
	TileInfo m_tile_info_cpu;
	std::shared_ptr<DescriptorLayout<cMovable>> m_movable_descriptor_layout;
	std::shared_ptr<DescriptorSet<cMovable>> m_movable_descriptor;




	void setup(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		m_movable_descriptor_layout = std::make_shared<DescriptorLayout<cMovable>>(context);

		m_info_cpu.m_data_size = 8192 / 2;
		m_info_cpu.m_active_counter = 0;
		m_tile_info_cpu.m_counter_max = 128;
		m_tile_info_cpu.m_resolusion = context->m_window->getClientSize<uvec2>();
		m_tile_info_cpu.m_tile_num = uvec2(32, 32);

		{
			m_movable_descriptor_layout = std::make_shared<DescriptorLayout<cMovable>>(context);

			cMovable movable;
			{
				{
					btr::BufferMemoryDescriptor desc;
					desc.size = sizeof(MovableInfo);
					movable.m_info = context->m_storage_memory.allocateMemory<MovableInfo>(desc);

					desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
					auto staging = context->m_staging_memory.allocateMemory<MovableInfo>(desc);
				}
				{
					btr::BufferMemoryDescriptor desc;
					desc.size = sizeof(MovableData)*m_info_cpu.m_data_size * 2;
					movable.m_data = context->m_storage_memory.allocateMemory<MovableData>(desc);
				}
				{
					btr::BufferMemoryDescriptorEx<TileInfo> desc;
					desc.element_num = 1;
					movable.m_tile_info = context->m_storage_memory.allocateMemory(desc);
				}
				{
					btr::BufferMemoryDescriptorEx<uint32_t> desc;
					desc.element_num = m_tile_info_cpu.m_tile_num.x*m_tile_info_cpu.m_tile_num.y;
					movable.m_tile_counter = context->m_storage_memory.allocateMemory(desc);
				}
				{
					btr::BufferMemoryDescriptorEx<uint32_t> desc;
					desc.element_num = m_tile_info_cpu.m_tile_num.x*m_tile_info_cpu.m_tile_num.y*m_tile_info_cpu.m_counter_max;
					movable.m_tile_index = context->m_storage_memory.allocateMemory(desc);
				}
			}
			m_movable_descriptor = m_movable_descriptor_layout->createDescriptorSet(context, std::move(movable));
		}

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "TileCulling.comp.spv",vk::ShaderStageFlagBits::eCompute },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
				m_shader_info[i].setModule(m_shader_module[i].get());
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		// setup pipeline_layout
		{
			std::vector<vk::DescriptorSetLayout> layouts =
			{
				m_movable_descriptor_layout->getLayout(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_CULLING] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}
		{
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info = {
				vk::ComputePipelineCreateInfo()
				.setStage(m_shader_info[SHADER_COMP_TILE_CULLING])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_CULLING].get()),
			};

			auto p = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PIPELINE_COMPUTE_CULLING] = std::move(p[0]);

		}

	}

	vk::CommandBuffer execute(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);


		cmd.end();
		return cmd;
	}

	vk::CommandBuffer draw(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		cmd.end();
		return cmd;
	}


};

