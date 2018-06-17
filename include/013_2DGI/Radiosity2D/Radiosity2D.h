#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
namespace rs2d
{

struct Radiosity2DContext
{
	enum
	{
		_BounceNum = 4, //!< ���C���ˉ�
		_Hierarchy_Num = 8,
		_Light_Num = 256,
	};
	int32_t RenderWidth;
	int32_t RenderHeight;
	int FragmentBufferSize;
	int BounceNum = _BounceNum;
	int Hierarchy_Num = _Hierarchy_Num;
	int Light_Num = _Light_Num;
	struct Info
	{
		mat4 m_camera_PV;
		uvec2 m_resolution;
		uvec2 m_emission_tile_size;
		uvec2 m_emission_tile_num;
		uvec2 _p;
		vec4 m_position;
		int m_fragment_map_hierarchy_offset[_Hierarchy_Num];

		int m_emission_tile_linklist_max;
		int m_emission_buffer_max;
	};

	struct Fragment
	{
		vec3 albedo;
		float _p;
	};

	struct LinkList
	{
		int32_t next;
		int32_t target;
	};

	Radiosity2DContext(const std::shared_ptr<btr::Context>& context)
	{
 		RenderWidth = 1024;
 		RenderHeight = 1024;
//		RenderWidth = 512;
//		RenderHeight = 512;
		FragmentBufferSize = RenderWidth * RenderHeight;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			btr::BufferMemoryDescriptorEx<Info> desc;
			desc.element_num = 1;
			u_fragment_info = context->m_uniform_memory.allocateMemory(desc);

			m_pm2d_info.m_position = vec4(0.f, 0.f, 0.f, 0.f);
			m_pm2d_info.m_resolution = uvec2(RenderWidth, RenderHeight);
			m_pm2d_info.m_emission_tile_size = uvec2(32, 32);
			m_pm2d_info.m_emission_tile_num = m_pm2d_info.m_resolution / m_pm2d_info.m_emission_tile_size;
			m_pm2d_info.m_camera_PV = glm::ortho(RenderWidth*-0.5f, RenderWidth*0.5f, RenderHeight*-0.5f, RenderHeight*0.5f, 0.f, 2000.f) * glm::lookAt(vec3(RenderWidth*0.5f, -1000.f, RenderHeight*0.5f)+m_pm2d_info.m_position.xyz(), vec3(RenderWidth*0.5f, 0.f, RenderHeight*0.5f) + m_pm2d_info.m_position.xyz(), vec3(0.f, 0.f, 1.f));

			int size = RenderHeight * RenderWidth / 64;
			m_pm2d_info.m_fragment_map_hierarchy_offset[0] = 0;
			m_pm2d_info.m_fragment_map_hierarchy_offset[1] = m_pm2d_info.m_fragment_map_hierarchy_offset[0] + size;
			m_pm2d_info.m_fragment_map_hierarchy_offset[2] = m_pm2d_info.m_fragment_map_hierarchy_offset[1] + size / (2 * 2);
			m_pm2d_info.m_fragment_map_hierarchy_offset[3] = m_pm2d_info.m_fragment_map_hierarchy_offset[2] + size / (4 * 4);
			m_pm2d_info.m_fragment_map_hierarchy_offset[4] = m_pm2d_info.m_fragment_map_hierarchy_offset[3] + size / (8 * 8);
			m_pm2d_info.m_fragment_map_hierarchy_offset[5] = m_pm2d_info.m_fragment_map_hierarchy_offset[4] + size / (16 * 16);
			m_pm2d_info.m_fragment_map_hierarchy_offset[6] = m_pm2d_info.m_fragment_map_hierarchy_offset[5] + size / (32 * 32);
			m_pm2d_info.m_fragment_map_hierarchy_offset[7] = m_pm2d_info.m_fragment_map_hierarchy_offset[6] + size / (64 * 64);

			m_pm2d_info.m_emission_tile_linklist_max = Light_Num * m_pm2d_info.m_emission_tile_num.x * m_pm2d_info.m_emission_tile_num.y;
			m_pm2d_info.m_emission_buffer_max = Light_Num;
			cmd.updateBuffer<Info>(u_fragment_info.getInfo().buffer, u_fragment_info.getInfo().offset, m_pm2d_info);
		}
		{
			btr::BufferMemoryDescriptorEx<Fragment> desc;
			desc.element_num = FragmentBufferSize;
			b_fragment_buffer = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int64_t> desc;
			int size = RenderHeight * RenderWidth / 64;
			desc.element_num = size;
			desc.element_num += size / (2 * 2);
			desc.element_num += size / (4 * 4);
			desc.element_num += size / (8 * 8);
			desc.element_num += size / (16 * 16);
			desc.element_num += size / (32 * 32);
			desc.element_num += size / (64 * 64);
			desc.element_num += size / (128 * 128);
			b_fragment_map = context->m_storage_memory.allocateMemory(desc);
		}

		{
			b_emission_reached = context->m_storage_memory.allocateMemory<uint64_t>({ (uint32_t)FragmentBufferSize/64 * 4 * Light_Num,{} });
			b_emission_occlusion = context->m_storage_memory.allocateMemory<uint64_t>({ (uint32_t)FragmentBufferSize/64 ,{} });
		}

		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
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
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(5),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(6),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(7),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(8),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(9),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(20),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(21),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(22),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(23),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(24),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(25),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(26),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(27),
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(28),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo uniforms[] = {
					u_fragment_info.getInfo(),
				};
				vk::DescriptorBufferInfo storages[] = {
					b_fragment_buffer.getInfo(),
					b_fragment_map.getInfo(),
				};
				vk::DescriptorBufferInfo emission_storages[] = {
					b_emission_reached.getInfo(),
					b_emission_occlusion.getInfo(),
				};

				vk::WriteDescriptorSet write[] = {
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(array_length(uniforms))
					.setPBufferInfo(uniforms)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(1)
					.setDstSet(m_descriptor_set.get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(emission_storages))
					.setPBufferInfo(emission_storages)
					.setDstBinding(20)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}
	}
	Info m_pm2d_info;

	btr::BufferMemoryEx<Info> u_fragment_info;
	btr::BufferMemoryEx<Fragment> b_fragment_buffer;
	btr::BufferMemoryEx<int64_t> b_fragment_map;

	btr::BufferMemoryEx<uint64_t> b_emission_reached;
	btr::BufferMemoryEx<uint64_t> b_emission_occlusion;
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};

struct PM2DPipeline
{
	virtual void execute(vk::CommandBuffer cmd) = 0;
};

}