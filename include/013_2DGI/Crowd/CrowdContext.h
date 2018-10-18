#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct CrowdContext
{
	struct CrowdInfo
	{
		uint32_t crowd_type_max;
		uint32_t crowd_max;
		uint32_t unit_max;
	};
	struct CrowdData
	{
		vec2 target;
		vec2 _p;
	};

	struct UnitInfo
	{
		float linear_speed;
		float angler_speed;
		float _p2;
		float _p3;
	};
	struct UnitData
	{
		vec2 m_pos;
		vec2 m_vel;
		int32_t crowd_id;
		int32_t unit_type;
		vec2 _p;
	};

	CrowdContext(const std::shared_ptr<btr::Context>& context)
	{
		m_context = context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			m_crowd_info.crowd_type_max = 16;
			m_crowd_info.crowd_max = 16;
			m_crowd_info.unit_max = 128;

//			m_unit_info[].angler_speed
		}
		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
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
			}
		}

		{
			u_crowd_info = m_context->m_uniform_memory.allocateMemory<CrowdInfo>({ 1, {} });
			u_unit_info = m_context->m_uniform_memory.allocateMemory<UnitInfo>({ m_crowd_info.crowd_max, {} });
			b_crowd = m_context->m_storage_memory.allocateMemory<CrowdData>({ m_crowd_info.crowd_max, {} });
			b_unit = m_context->m_storage_memory.allocateMemory<UnitData>({ m_crowd_info.unit_max*2, {} });
			b_unit_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 1, {} });
			b_grid_counter = m_context->m_storage_memory.allocateMemory<uint32_t>({ 1024*1024/*m_gi2d_context->FragmentBufferSize*/, {} });

			vk::DescriptorBufferInfo uniforms[] = {
				u_crowd_info.getInfo(),
				u_unit_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				b_crowd.getInfo(),
				b_unit.getInfo(),
				b_unit_counter.getInfo(),
				b_grid_counter.getInfo(),
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
				.setDstBinding(2)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}


		// initialize
		{
			{
				auto staging = m_context->m_staging_memory.allocateMemory<CrowdInfo>({ 1, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
				*staging.getMappedPtr() = m_crowd_info;

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(u_crowd_info.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, u_crowd_info.getInfo().buffer, copy);

			}

			{
				auto staging = m_context->m_staging_memory.allocateMemory<UnitData>({ m_crowd_info.unit_max * 2, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
				for (int32_t i = 0; i < m_crowd_info.unit_max * 2; i++)
				{
					staging.getMappedPtr(i)->m_pos = abs(glm::ballRand(4000.f).xy());
//					staging.getMappedPtr(i)->m_pos = vec2(250.f);
					staging.getMappedPtr(i)->m_vel = glm::ballRand(2.f).xy();
					staging.getMappedPtr(i)->unit_type = 0;
					staging.getMappedPtr(i)->crowd_id = 0;
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(b_unit.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, b_unit.getInfo().buffer, copy);

			}
		}
	}

	void execute(vk::CommandBuffer cmd)
	{

	}

	std::shared_ptr<btr::Context> m_context;
	CrowdInfo m_crowd_info;
//	std::array<UnitInfo, 16> m_unit_info;

	btr::BufferMemoryEx<CrowdInfo> u_crowd_info;
	btr::BufferMemoryEx<UnitInfo> u_unit_info;
	btr::BufferMemoryEx<CrowdData> b_crowd;
	btr::BufferMemoryEx<UnitData> b_unit;
	btr::BufferMemoryEx<uvec4> b_unit_counter;
	btr::BufferMemoryEx<uint32_t> b_grid_counter;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};
