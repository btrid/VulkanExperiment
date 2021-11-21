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
			update_desc.frame_max = sGlobal::FRAME_COUNT_MAX;
			update_desc.element_num = 2;
			m_camera.setup(update_desc);
			m_camera_frustom.setup(update_desc);
		}
		{
			auto stage = vk::ShaderStageFlagBits::eAll;
			vk::DescriptorSetLayoutBinding bindings[] =
			{
				vk::DescriptorSetLayoutBinding().setBinding(0).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eUniformBuffer).setStageFlags(stage),
				vk::DescriptorSetLayoutBinding().setBinding(1).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eUniformBuffer).setStageFlags(stage),
			};
			vk::DescriptorSetLayoutCreateInfo dslCI = vk::DescriptorSetLayoutCreateInfo().setBindingCount(array_size(bindings)).setPBindings(bindings);
			m_descriptor_set_layout[DESCRIPTOR_SET_CAMERA] = context->m_device.createDescriptorSetLayout(dslCI);
		}

		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = context->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = (uint32_t)m_descriptor_set_layout.size();
		alloc_info.pSetLayouts = m_descriptor_set_layout.data();
		auto descriptor_set = context->m_device.allocateDescriptorSets(alloc_info);
		std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
		{

			vk::DescriptorBufferInfo uniforms[] = {
				m_camera.getInfo(),
				m_camera_frustom.getInfo(),
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
			context->m_device.updateDescriptorSets(write_desc, {});
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
		auto& device = context->m_device;
		auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

		std::vector<vk::BufferMemoryBarrier> to_transfer = 
		{
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
		cmd.copyBuffer(m_camera.getStagingBufferInfo().buffer, m_camera.getInfo().buffer, array_length(copy_info), copy_info);

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