#pragma once

#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>

struct sCameraManager : public Singleton<sCameraManager>
{
	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_CAMERA,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_CAMERA,
		DESCRIPTOR_SET_NUM,
	};

	btr::UpdateBuffer<CameraGPU> m_camera;
	btr::UpdateBuffer<CameraFrustomGPU> m_camera_frustom;

	std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::DescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	void setup(const std::shared_ptr<btr::Context>& context)
	{
		{
			btr::UpdateBufferDescriptor update_desc;
			update_desc.device_memory = context->m_uniform_memory;
			update_desc.staging_memory = context->m_staging_memory;
			update_desc.frame_max = sGlobal::FRAME_MAX;
			update_desc.element_num = 2;
			m_camera.setup(update_desc);
			m_camera_frustom.setup(update_desc);
		}

		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		auto stage = vk::ShaderStageFlagBits::eAll;
		bindings[DESCRIPTOR_SET_LAYOUT_CAMERA] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(stage)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(1),
		};
		for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount((uint32_t)bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = context->m_device->createDescriptorSetLayout(descriptor_set_layout_info);
		}

		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = context->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = (uint32_t)m_descriptor_set_layout.size();
		alloc_info.pSetLayouts = m_descriptor_set_layout.data();
		auto descriptor_set = context->m_device->allocateDescriptorSets(alloc_info);
		std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
		{

			vk::DescriptorBufferInfo uniforms[] = {
				m_camera.getBufferInfo(),
				m_camera_frustom.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LAYOUT_CAMERA]),
			};
			context->m_device->updateDescriptorSets(write_desc, {});
		}

	}

	void sync()
	{
		for (auto& c : cCamera::sCamera::Order().getCameraList())
		{
			c->getRenderData() = c->getData();
		}
		
	}
	vk::CommandBuffer draw(const std::shared_ptr<btr::Context>& context)
	{
		auto& device = context->m_gpu.getDevice();
		auto cmd = context->m_cmd_pool->allocCmdOnetime(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

		std::vector<vk::BufferMemoryBarrier> to_transfer = {
			m_camera.getBufferMemory().makeMemoryBarrierEx().setDstAccessMask(vk::AccessFlagBits::eTransferWrite),
			m_camera_frustom.getBufferMemory().makeMemoryBarrierEx().setDstAccessMask(vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		for (uint32_t i = 0 ; i < m_camera.getDescriptor().element_num; i++)
		{
			if (i >= (uint32_t)cCamera::sCamera::Order().getCameraList().size())
			{
				break;
			}

			auto& camera = cCamera::sCamera::Order().getCameraList()[i];
			CameraGPU camera_GPU;
			camera_GPU.setup(*camera.get());
			CameraFrustomGPU camera_frustom;
			camera_frustom.setup(*camera.get());

			m_camera.subupdate(&camera_GPU, 1, i, context->getGPUFrame());
			m_camera_frustom.subupdate(&camera_frustom, 1, i, context->getGPUFrame());
		}

		vk::BufferCopy copy_info[] = { m_camera.update(context->getGPUFrame()), m_camera_frustom.update(context->getGPUFrame()) };
		cmd.copyBuffer(m_camera.getStagingBufferInfo().buffer, m_camera.getBufferInfo().buffer, array_length(copy_info), copy_info);

		std::vector<vk::BufferMemoryBarrier> to_draw_barrier = {
			m_camera.getBufferMemory().makeMemoryBarrierEx().setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eShaderRead),
			m_camera_frustom.getBufferMemory().makeMemoryBarrierEx().setSrcAccessMask(vk::AccessFlagBits::eTransferWrite).setDstAccessMask(vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_draw_barrier }, {});

		cmd.end();
		return cmd;

	}

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor]; }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i]; }

};