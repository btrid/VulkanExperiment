#pragma once

#include <btrlib/Define.h>
#include <btrlib/loader.h>
#include <btrlib/cCamera.h>
#include <btrlib/BufferMemory.h>
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


	std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;
	std::array<vk::DescriptorSet, DESCRIPTOR_SET_NUM> m_descriptor_set;

	void setup(std::shared_ptr<btr::Loader>& loader)
	{
		{

			btr::UpdateBufferDescriptor update_desc;
			update_desc.device_memory = loader->m_uniform_memory;
			update_desc.staging_memory = loader->m_staging_memory;
			update_desc.frame_max = sGlobal::FRAME_MAX;
			m_camera.setup(update_desc);
		}

		std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
		bindings[DESCRIPTOR_SET_LAYOUT_CAMERA] =
		{
			vk::DescriptorSetLayoutBinding()
			.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
			.setDescriptorCount(1)
			.setDescriptorType(vk::DescriptorType::eUniformBuffer)
			.setBinding(0),
		};
		for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
		{
			vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
				.setBindingCount(bindings[i].size())
				.setPBindings(bindings[i].data());
			m_descriptor_set_layout[i] = loader->m_device->createDescriptorSetLayout(descriptor_set_layout_info);
		}

		vk::DescriptorSetAllocateInfo alloc_info;
		alloc_info.descriptorPool = loader->m_descriptor_pool.get();
		alloc_info.descriptorSetCount = m_descriptor_set_layout.size();
		alloc_info.pSetLayouts = m_descriptor_set_layout.data();
		auto descriptor_set = loader->m_device->allocateDescriptorSets(alloc_info);
		std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
		{

			std::vector<vk::DescriptorBufferInfo> uniforms = {
				m_camera.getBufferInfo(),
			};
			std::vector<vk::WriteDescriptorSet> write_desc =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(uniforms.size())
				.setPBufferInfo(uniforms.data())
				.setDstBinding(0)
				.setDstSet(m_descriptor_set[DESCRIPTOR_SET_LAYOUT_CAMERA]),
			};
			loader->m_device->updateDescriptorSets(write_desc, {});
		}

	}

	void execute()
	{
// 		for (auto* c : cCamera::sCamera::Order().getCameraList())
// 		{
// 			c->getRenderData() = c->getData();
// 		}
	}
	void sync()
	{
		for (auto* c : cCamera::sCamera::Order().getCameraList())
		{
			c->getRenderData() = c->getData();
		}
		
	}
	vk::CommandBuffer draw(std::shared_ptr<btr::Executer>& executer)
	{
		auto& device = executer->m_gpu.getDevice();
		auto cmd = executer->m_cmd_pool->allocCmdOnetime(device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics));

		std::vector<vk::BufferMemoryBarrier> to_transfer = {
			m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		auto* camera = cCamera::sCamera::Order().getCameraList()[0];
		CameraGPU camera_GPU;
		camera_GPU.setup(*camera);
		m_camera.subupdate(camera_GPU);
		m_camera.update(cmd);

		std::vector<vk::BufferMemoryBarrier> to_draw_barrier = {
			m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_draw_barrier }, {});

		cmd.end();
		return cmd;

	}

	vk::DescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayout desctiptor)const { return m_descriptor_set_layout[desctiptor]; }
	vk::DescriptorSet getDescriptorSet(DescriptorSet i)const { return m_descriptor_set[i]; }

};