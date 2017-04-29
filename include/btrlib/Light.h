#pragma once
#include <array>
#include <memory>
#include <mutex>
#include <btrlib/Define.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/Singleton.h>
namespace btr
{

struct LightParam {
	glm::vec4 m_position;
	glm::vec4 m_emission;
};
struct Light
{

	virtual bool update()
	{
		return true;
	}

	virtual LightParam getParam()const 
	{
		return LightParam();
	}

	struct Manager
	{
		struct Private
		{
			cDevice m_device;
			AllocatedMemory m_memory;

			vk::Buffer m_staging_buffer;
			LightParam* m_staging_data_ptr;
			vk::DeviceMemory m_staging_memory;
			vk::DeviceSize m_size;
			uint32_t m_light_num;

			std::vector<std::unique_ptr<Light>> m_light;
			std::vector<std::unique_ptr<Light>> m_light_new;
			std::mutex m_light_new_mutex;
		};
		std::unique_ptr<Private> m_private;
		void setup(btr::BufferMemory& memory, size_t light_num)
		{
			m_private = std::make_unique<Private>();
			m_private->m_device = memory.getDevice();
			m_private->m_size = sizeof(LightParam) * light_num;
			m_private->m_light_num = light_num;
			m_private->m_memory = memory.allocateMemory(m_private->m_size);
			
			vk::BufferCreateInfo staging_info;
			staging_info.usage = vk::BufferUsageFlagBits::eTransferSrc;
			staging_info.size = m_private->m_size * sGlobal::FRAME_MAX;
			staging_info.sharingMode = vk::SharingMode::eExclusive;
			m_private->m_staging_buffer = m_private->m_device->createBuffer(staging_info);

			vk::MemoryRequirements staging_memory_request = m_private->m_device->getBufferMemoryRequirements(m_private->m_staging_buffer);
			vk::MemoryAllocateInfo staging_memory_alloc_info;
			staging_memory_alloc_info.allocationSize = staging_memory_request.size;
			staging_memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(m_private->m_device.getGPU(), staging_memory_request, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

			m_private->m_staging_memory = m_private->m_device->allocateMemory(staging_memory_alloc_info);
			m_private->m_device->bindBufferMemory(m_private->m_staging_buffer, m_private->m_staging_memory, 0);

			m_private->m_staging_data_ptr = static_cast<LightParam*>(m_private->m_device->mapMemory(m_private->m_staging_memory, 0, m_private->m_size, vk::MemoryMapFlags()));
		}

		void add(std::unique_ptr<Light>&& light)
		{
			m_private->m_light_new.push_back(std::move(light));
		}
		void execute(vk::CommandBuffer cmd)
		{
			{
				std::vector<std::unique_ptr<Light>> light;
				{
					std::lock_guard<std::mutex> lock(m_private->m_light_new_mutex);
					light = std::move(m_private->m_light_new);
				}
				m_private->m_light.insert(m_private->m_light.end(), std::make_move_iterator(light.begin()), std::make_move_iterator(light.end()));
			}

			m_private->m_light.erase(std::remove_if(m_private->m_light.begin(), m_private->m_light.end(), [](auto& p) { return !p->update(); }), m_private->m_light.end());

			size_t frame_offset = sGlobal::Order().getCurrentFrame() * m_private->m_light_num;
			auto* p = m_private->m_staging_data_ptr + frame_offset;
			uint32_t index = 0;
			for (auto& it : m_private->m_light)
			{
				assert(index < m_private->m_light_num);
				p[index] = it->getParam();
				index++;
			}

			vk::BufferCopy copy_info;
			copy_info.size = index * sizeof(LightParam);
			copy_info.srcOffset = frame_offset;
			copy_info.dstOffset = 0;

			vk::BufferMemoryBarrier to_copy_barrier;
			to_copy_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_copy_barrier.buffer = m_private->m_memory.getBuffer();
			to_copy_barrier.setOffset(0);
			to_copy_barrier.setSize(copy_info.size);
			to_copy_barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			vk::BufferMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.dstQueueFamilyIndex = m_private->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.buffer = m_private->m_memory.getBuffer();
			to_shader_read_barrier.setOffset(0);
			to_shader_read_barrier.setSize(copy_info.size);
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, { to_copy_barrier }, {});
			cmd.copyBuffer(m_private->m_staging_buffer, m_private->m_memory.getBuffer(), copy_info);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTopOfPipe, vk::DependencyFlags(), {}, { to_shader_read_barrier }, {});

		}
	};
};

}

