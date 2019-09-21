#include <btrlib/System.h>
#include <btrlib/Context.h>
#include <btrlib/cWindow.h>

#include <applib/App.h>

namespace btr
{

System::System(const std::shared_ptr<btr::Context>& context)
{
	m_context = context;
	// memory allocate
	{
		m_buffer = context->m_uniform_memory.allocateMemory<SystemData>({ 1, {} });
		for (auto& staging : m_staging)
		{
			staging = context->m_staging_memory.allocateMemory<SystemData>({ 1,{} });
		}
	}

	// create descriptor layout
	{
		std::vector<vk::DescriptorSetLayoutBinding> desc =
		{
			vk::DescriptorSetLayoutBinding()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
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
			m_buffer.getInfo(),
		};
		std::vector<vk::WriteDescriptorSet> write_desc =
		{
			vk::WriteDescriptorSet()
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setDescriptorCount(array_length(uniforms))
			.setPBufferInfo(uniforms)
			.setDstBinding(0)
			.setDstSet(m_descriptor_set.get()),
		};
		context->m_device.updateDescriptorSets(write_desc, {});
	}
}

void System::execute(vk::CommandBuffer cmd)
{
	SystemData data = {};
//	for (uint32_t i = 0; i < app::g_app_instance->m_window_list.size(); i++)
	{
//		auto& window = app::g_app_instance->m_window_list[i];
		auto& window = m_context->m_window;
		data.m_render_index = sGlobal::Order().getRenderIndex();
		data.m_render_frame = sGlobal::Order().getRenderFrame();
		data.m_deltatime = sGlobal::Order().getDeltaTime();
//		data.m_resolution = window->getClientSize();
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
			auto& keyboard = m_context->m_window->getInput().m_keyboard;
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
	*m_staging[m_context->getCPUFrame()].getMappedPtr() = data;

	auto to_transfer = m_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

	auto staging = m_staging[m_context->getCPUFrame()].getInfo();
	vk::BufferCopy copy;
	copy.setSrcOffset(staging.offset);
	copy.setSize(staging.range);
	copy.setSrcOffset(m_buffer.getInfo().offset);
	cmd.copyBuffer(staging.buffer, m_buffer.getInfo().buffer, copy);

	auto to_read = m_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {}, {}, to_read, {});
}

}
