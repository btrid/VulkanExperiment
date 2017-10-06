#pragma once

#include <memory>
#include <btrlib/Context.h>
#include <btrlib/AllocatedMemory.h>

struct SystemDescriptor : public UniqueDescriptorModule
{
	SystemDescriptor(const std::shared_ptr<btr::Context>& context)
	{
		// memory allocate
		{
			btr::UpdateBufferDescriptor desc;
			desc.device_memory = context->m_uniform_memory;
			desc.staging_memory = context->m_staging_memory;
			desc.frame_max = sGlobal::FRAME_MAX;
			desc.element_num = 1;
			m_data.setup(desc);
		}

		// create descriptor
		{
			std::vector<vk::DescriptorSetLayoutBinding> desc =
			{
				vk::DescriptorSetLayoutBinding()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setStageFlags(vk::ShaderStageFlagBits::eAll)
				.setDescriptorCount(1)
				.setBinding(0),
			};
			createDescriptor(context, desc);

		}

		// update descriptor
		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_data.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(write_desc, {});
		}

	}

public:
	struct SystemData
	{
		uint32_t m_gpu_index;
		float m_deltatime;
		uint32_t m_gpu_frame;
		uint32_t m_is_key_on;
		uint32_t m_is_key_off;
		uint32_t m_is_key_hold;
	};
	btr::UpdateBuffer<SystemData> m_data;
};

struct sSystem : public Singleton<sSystem>
{
	friend class Singleton<sSystem>;

	void setup(const std::shared_ptr<btr::Context>& context)
	{
		m_system_descriptor = std::make_shared<SystemDescriptor>(context);
	}

	vk::CommandBuffer execute(const std::shared_ptr<btr::Context>& context)
	{
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		SystemDescriptor::SystemData data;
		data.m_gpu_index = sGlobal::Order().getGPUIndex();
		data.m_deltatime = sGlobal::Order().getDeltaTime();
		data.m_gpu_frame = context->getGPUFrame();
		auto& keyboard = context->m_window->getInput().m_keyboard;
		struct
		{
			char key;
			uint32_t key_bit;
		} key_mapping[] =
		{
			VK_UP, 0,
			VK_DOWN, 1,
			VK_RIGHT, 2,
			VK_LEFT, 3,
			'l', 4,
			'k', 5,
		};

		data.m_is_key_on = data.m_is_key_off = data.m_is_key_hold = 0;
		for (uint32_t i = 0; i < array_length(key_mapping); i++)
		{
			data.m_is_key_on |= keyboard.isOn(key_mapping[i].key) ? (1 << key_mapping[i].key_bit) : 0;
			data.m_is_key_off |= keyboard.isOff(key_mapping[i].key) ? (1 << key_mapping[i].key_bit) : 0;
			data.m_is_key_hold |= keyboard.isHold(key_mapping[i].key) ? (1 << key_mapping[i].key_bit) : 0;
		}

		m_system_descriptor->m_data.subupdate(&data, 1, 0, sGlobal::Order().getGPUIndex());
		auto copy_info = m_system_descriptor->m_data.update(sGlobal::Order().getGPUIndex());

		auto to_transfer = m_system_descriptor->m_data.getBufferMemory().makeMemoryBarrierEx();
		to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		cmd.copyBuffer(m_system_descriptor->m_data.getStagingBufferInfo().buffer, m_system_descriptor->m_data.getBufferInfo().buffer, copy_info);

		auto to_read = m_system_descriptor->m_data.getBufferMemory().makeMemoryBarrierEx();
		to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
		to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, to_read, {});

		cmd.end();
		return cmd;
	}

	std::shared_ptr<SystemDescriptor> m_system_descriptor;
public:
	const SystemDescriptor& getSystemDescriptor()const { return *m_system_descriptor; }

};

