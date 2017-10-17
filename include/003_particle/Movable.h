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
	vec4 pos_scale;
	uint32_t type;
};

struct cMovable
{
	btr::BufferMemoryEx<MovableInfo> m_info;
	btr::BufferMemoryEx<MovableData> m_data;

	void updateDescriptor(const std::shared_ptr<btr::Context>& context, vk::DescriptorSet descriptor_set)
	{
		vk::DescriptorBufferInfo storages[] =
		{
			m_info.getBufferInfo(),
			m_data.getBufferInfo(),
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
		};
		desc.m_set_num = 10;
		return desc;
	}
};

struct sMovable : Singleton<sMovable>
{
	friend Singleton<sMovable>;

	MovableInfo m_info_cpu;
	std::shared_ptr<DescriptorLayout<cMovable>> m_movable_descriptor_layout;
	std::shared_ptr<DescriptorSet<cMovable>> m_movable_descriptor;

	void setup(std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		m_movable_descriptor_layout = std::make_shared<DescriptorLayout<cMovable>>(context);

		m_info_cpu.m_data_size = 8192 / 2;
		m_info_cpu.m_active_counter = 0;

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
			}
			m_movable_descriptor = m_movable_descriptor_layout->createDescriptorSet(context, std::move(movable));
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

