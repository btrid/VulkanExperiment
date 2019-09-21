#include <applib/sSystem.h>
#include <btrlib/Context.h>
#include <btrlib/cWindow.h>

#include <applib/App.h>

sSystem::sSystem(const std::shared_ptr<btr::Context>& context)
{
	// memory allocate
	{
		btr::UpdateBufferDescriptor desc;
		desc.device_memory = context->m_uniform_memory;
		desc.staging_memory = context->m_staging_memory;
		desc.frame_max = sGlobal::FRAME_COUNT_MAX;
		desc.element_num = 16;
		m_data.setup(context->m_gpu, desc);
	}

	// create descriptor layout
	{
		std::vector<vk::DescriptorSetLayoutBinding> desc =
		{
			vk::DescriptorSetLayoutBinding()
			.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
			.setStageFlags(vk::ShaderStageFlagBits::eAll)
			.setDescriptorCount(1)
			.setBinding(0),
		};
		m_descriptor_layout = btr::Descriptor::createDescriptorSetLayout(context, desc);
	}

	// create descriptor set
	{
		m_descriptor_set = btr::Descriptor::allocateDescriptorSet(context, context->m_descriptor_pool.get(), m_descriptor_layout.get());
	}
	// update descriptor
	{

		vk::DescriptorBufferInfo uniforms[] = {
			m_data.getBufferInfo(),
		};
		std::vector<vk::WriteDescriptorSet> write_desc =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBufferDynamic)
			.setDescriptorCount(array_length(uniforms))
			.setPBufferInfo(uniforms)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set.get()),
		};
		context->m_device->updateDescriptorSets(write_desc, {});
	}
}

vk::CommandBuffer sSystem::execute(const std::shared_ptr<btr::Context>& context)
{
	auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
//	auto* data_list = m_data.mapSubBuffer(sGlobal::Order().getRenderIndex(), 0);
	SystemData data_list[16] = {};
	for (uint32_t i = 0; i < app::g_app_instance->m_window_list.size(); i++)
	{
		auto& window = app::g_app_instance->m_window_list[i];
		auto& data = data_list[i];
		data.m_render_index = sGlobal::Order().getRenderIndex();
		data.m_render_frame = sGlobal::Order().getRenderFrame();
		data.m_deltatime = sGlobal::Order().getDeltaTime();
		data.m_resolution.x = window->getFrontBuffer()->m_info.extent.width;
		data.m_resolution.y = window->getFrontBuffer()->m_info.extent.height;
		{
			auto& mouse = window->getInput().m_mouse;
			data.m_is_mouse_on = data.m_is_mouse_off = data.m_is_mouse_hold = 0;
			for (uint32_t i = 0; i < cMouse::BUTTON_NUM; i++)
			{
				data.m_is_mouse_on |= mouse.isOn((cMouse::Button)i) ? (1 << (i * 3)) : 0;
				data.m_is_mouse_on |= mouse.isHold((cMouse::Button)i) ? (1 << (i * 3 + 1)) : 0;
				data.m_is_mouse_on |= mouse.isOff((cMouse::Button)i) ? (1 << (i * 3 + 2)) : 0;
			}
			data.m_touch_time = mouse.m_param[0].time;
			data.m_mouse_position = mouse.xy;
			data.m_mouse_position_old = mouse.xy_old;
		}

		{
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
		}

	}

	m_data.subupdate(data_list, 16, 0, sGlobal::Order().getRenderIndex());
	auto copy_info = m_data.update(sGlobal::Order().getRenderIndex());

	auto to_transfer = m_data.getBufferMemory().makeMemoryBarrierEx();
	to_transfer.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

	cmd.copyBuffer(m_data.getStagingBufferInfo().buffer, m_data.getBufferInfo().buffer, copy_info);

	auto to_read = m_data.getBufferMemory().makeMemoryBarrierEx();
	to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
	to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, to_read, {});

	cmd.end();
	return cmd;
}
