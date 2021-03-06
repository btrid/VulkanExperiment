#include <010_sound/Sound.h>

#include <010_sound/sSoundSystem.h>

SoundBuffer::SoundBuffer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<rWave>& wave)
{
	const auto* play_format = sSoundSystem::Order().getFormat();
	std::vector<int32_t> buf(((float)play_format->nSamplesPerSec / wave->getFormat()->nSamplesPerSec) * wave->getLength() / 2);

	auto* src_data = (int16_t*)wave->getData();
	float rate = (float)wave->getFormat()->nSamplesPerSec / play_format->nSamplesPerSec;
	int32_t dst_index = 0;
	for (int32_t dst_index = 0; dst_index < buf.size(); dst_index++)
	{
		float src_index = dst_index * rate;
		float lerp_rate = src_index - floor(src_index);
		int i = floor(src_index);
		buf[dst_index] = glm::lerp<float>(src_data[i], src_data[i + 1], lerp_rate) * 50000;
	}

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	{
		btr::BufferMemoryDescriptorEx<int32_t> desc;
		desc.element_num = buf.size();
		m_buffer = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);
		memcpy(staging.getMappedPtr(), buf.data(), vector_sizeof(buf));


		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_buffer.getInfo().offset);
		copy.setSize(staging.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, m_buffer.getInfo().buffer, copy);
		{
			auto to_read = m_buffer.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
	}
	{
		btr::BufferMemoryDescriptorEx<SoundInfo> desc;
		desc.element_num = 1;
		m_info = context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = context->m_staging_memory.allocateMemory(desc);

		SoundInfo info;
		info.m_length = buf.size();
		info.m_samples_per_sec = play_format->nSamplesPerSec;
		memcpy(staging.getMappedPtr(), &info, sizeof(info));

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_info.getInfo().offset);
		copy.setSize(staging.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, m_info.getInfo().buffer, copy);

		{
			auto to_read = m_info.makeMemoryBarrier();
			to_read.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			to_read.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});
		}
	}

}
