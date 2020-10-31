#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct CrowdContext
{
	enum {
	};

	struct CrowdInfo
	{
		uint32_t crowd_info_max;
		uint32_t unit_info_max;
		uint32_t crowd_data_max;
		uint32_t unit_data_max;

	};

	struct CrowdScene
	{
		float m_deltatime;
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
		float m_rot;
		float m_rot_prev;

		float m_move;
		int32_t crowd_type;
		int32_t unit_type;
		float _p;
	};

	CrowdContext(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{
			m_crowd_info.crowd_info_max = 1;
			m_crowd_info.unit_info_max = 16;
			m_crowd_info.crowd_data_max = 16;
			m_crowd_info.unit_data_max = 64*16;

			m_crowd_scene.m_deltatime= 0.f;
//			m_crowd_scene.m_skip = 0;
		}
		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(20, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(21, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(22, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			}
		}

		{
			auto link_size = gi2d_context->m_gi2d_info.m_resolution.x*gi2d_context->m_gi2d_info.m_resolution.y / (32*32);
			u_crowd_info = m_context->m_uniform_memory.allocateMemory<CrowdInfo>({ 1, {} });
			u_crowd_scene = m_context->m_uniform_memory.allocateMemory<CrowdScene>({ 1, {} });
			u_unit_info = m_context->m_uniform_memory.allocateMemory<UnitInfo>({ m_crowd_info.unit_info_max, {} });
			b_crowd = m_context->m_storage_memory.allocateMemory<CrowdData>({ m_crowd_info.crowd_data_max, {} });
			b_unit_pos = m_context->m_storage_memory.allocateMemory<vec4>({ m_crowd_info.unit_data_max, {} });
			b_unit_move = m_context->m_storage_memory.allocateMemory<vec2>({ m_crowd_info.unit_data_max, {} });
			b_unit_counter = m_context->m_storage_memory.allocateMemory<uvec4>({ 1, {} });
			b_unit_link_head = m_context->m_storage_memory.allocateMemory<int32_t>(link_size);
			b_unit_link_next = m_context->m_storage_memory.allocateMemory<int32_t>(m_crowd_info.unit_data_max);


			vk::DescriptorBufferInfo uniforms[] = {
				u_crowd_info.getInfo(),
				u_crowd_scene.getInfo(),
				u_unit_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				b_crowd.getInfo(),
				b_unit_counter.getInfo(),
				b_unit_link_head.getInfo(),
				b_unit_link_next.getInfo(),
			};
			vk::DescriptorBufferInfo storages_data[] = {
				b_unit_pos.getInfo(),
				b_unit_move.getInfo(),
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
				.setDstBinding(3)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages_data))
				.setPBufferInfo(storages_data)
				.setDstBinding(20)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
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
				auto staging = m_context->m_staging_memory.allocateMemory<UnitData>({ m_crowd_info.unit_data_max * 2, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
				std::vector<vec4> pos(m_crowd_info.unit_data_max);
				std::vector<vec2> rot(m_crowd_info.unit_data_max);
				for (int32_t i = 0; i < m_crowd_info.unit_data_max; i++)
				{
					auto p = abs(glm::ballRand(800.f).xy()) + vec2(200.f);
					float r = (std::rand() % 628) * 0.01f;
					pos[i] = vec4(p, r, 0.f);
					rot[i] = vec2(0.f);
				}

				cmd.updateBuffer<vec4>(b_unit_pos.getInfo().buffer, b_unit_pos.getInfo().offset, pos);
				cmd.updateBuffer<vec2>(b_unit_move.getInfo().buffer, b_unit_move.getInfo().offset, rot);

			}

			{
				auto staging = m_context->m_staging_memory.allocateMemory<UnitInfo>({ m_crowd_info.unit_info_max, btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT });
				for (int32_t i = 0; i < m_crowd_info.unit_info_max; i++)
				{
					auto& info = *staging.getMappedPtr(i);
					info.linear_speed = 5.f;
					info.angler_speed = 500.5f;
				}

				vk::BufferCopy copy;
				copy.setSrcOffset(staging.getInfo().offset);
				copy.setDstOffset(u_unit_info.getInfo().offset);
				copy.setSize(staging.getInfo().range);
				cmd.copyBuffer(staging.getInfo().buffer, u_unit_info.getInfo().buffer, copy);

			}

			vk::BufferMemoryBarrier to_read[] = {
				u_crowd_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				u_unit_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_unit_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				b_unit_move.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});
		}
	}

	void execute(vk::CommandBuffer cmd)
	{
		{
			vk::BufferMemoryBarrier to_write[] = {
				u_crowd_scene.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, 0, {}, array_length(to_write), to_write, 0, {});
		}
		m_crowd_scene.m_deltatime = sGlobal::Order().getDeltaTime();
		cmd.updateBuffer<CrowdScene>(u_crowd_scene.getInfo().buffer, u_crowd_scene.getInfo().offset, m_crowd_scene);
		{

			vk::BufferMemoryBarrier to_read[] = 
			{
				u_crowd_scene.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, {}, array_length(to_read), to_read, 0, {});

		}
	}

	std::shared_ptr<btr::Context> m_context;
	CrowdInfo m_crowd_info;
	CrowdScene m_crowd_scene;
 
	btr::BufferMemoryEx<CrowdInfo> u_crowd_info;
	btr::BufferMemoryEx<CrowdScene> u_crowd_scene;
	btr::BufferMemoryEx<UnitInfo> u_unit_info;
	btr::BufferMemoryEx<CrowdData> b_crowd;
	btr::BufferMemoryEx<vec4> b_unit_pos;
	btr::BufferMemoryEx<vec2> b_unit_move;
	btr::BufferMemoryEx<uvec4> b_unit_counter;
	btr::BufferMemoryEx<int32_t> b_unit_link_head;
	btr::BufferMemoryEx<int32_t> b_unit_link_next;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }
};
