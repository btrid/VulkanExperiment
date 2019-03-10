#pragma once
#include "GI2DRigidbody.h"
#include "GI2DPhysicsWorld.h"

GI2DRigidbody::GI2DRigidbody(const std::shared_ptr<PhysicsWorld>& world, int32_t px, int32_t py)
{
	auto cmd = world->m_context->m_cmd_pool->allocCmdTempolary(0);

	m_particle_num = px * py;

	{
		{
			b_rigidbody = world->m_context->m_storage_memory.allocateMemory<Rigidbody>({ 1,{} });

			b_relative_pos = world->m_context->m_storage_memory.allocateMemory<vec2>({ m_particle_num,{} });
			b_rbparticle = world->m_context->m_storage_memory.allocateMemory<rbParticle>({ m_particle_num,{} });
			b_rbpos_bit = world->m_context->m_storage_memory.allocateMemory<uint64_t>({ 4 * 4,{} });
			{

				std::vector<vec2> pos(m_particle_num);
				std::vector<vec2> rela_pos(m_particle_num);
				std::vector<rbParticle> pstate(m_particle_num);
				std::vector<uint64_t> bit(4 * 4);
				vec2 center = vec2(0.f);

				uint32_t contact_index = 0;
				for (int y = 0; y < py; y++)
				{
					for (int x = 0; x < px; x++)
					{
						pos[x + y * px].x = 123.f + x;
						pos[x + y * px].y = 220.f + y;

						if (y == 0 || y == py - 1 || x == 0 || x == px - 1)
						{
							pstate[x + y * px].contact_index = contact_index++;
						}
						else
						{
							pstate[x + y * px].contact_index = -1;
						}
						pstate[x + y * px].is_contact = 0;
						//							bit[x / 8 + y / 8 * 4] |= 1ull << (x % 8 + (y % 8) * 8);
					}
				}
				for (auto& p : pos) {
					center += p;
				}
				center /= pos.size();

				vec2 size = vec2(0.f);
				vec2 size_max = vec2(0.f);
				for (int32_t i = 0; i < rela_pos.size(); i++)
				{
					rela_pos[i] = pos[i] - center;
					size += rela_pos[i];
					size_max = glm::max(abs(rela_pos[i]), size_max);
				}

				size /= pos.size();
				cmd.updateBuffer<vec2>(b_relative_pos.getInfo().buffer, b_relative_pos.getInfo().offset, rela_pos);
				cmd.updateBuffer<rbParticle>(b_rbparticle.getInfo().buffer, b_rbparticle.getInfo().offset, pstate);

				float inertia = 0.f;
				for (int32_t i = 0; i < rela_pos.size(); i++)
				{
					//						if (pstate[i].contact_index >= 0)
					{
						inertia += dot(rela_pos[i], rela_pos[i]) /** mass*/;
					}
				}
				inertia /= 12.f;
				Rigidbody rb;
				rb.pos = center;
				rb.pos_old = rb.pos;
				rb.center = size / 2.f;
				rb.inertia = inertia;
				rb.size = size_max;
				rb.vel = vec2(0.f);
				rb.pos_work = ivec2(0.f);
				rb.vel_work = ivec2(0.f);
				rb.angle_vel_work = 0.f;
				rb.pnum = m_particle_num;
				rb.angle = 3.14f / 4.f + 0.2f;
//					rb.angle = 0.f;
				rb.angle_vel = 0.f;
				rb.solver_count = 0;
				rb.dist = -1;
				rb.damping_work = ivec2(0.f);

				cmd.updateBuffer<Rigidbody>(b_rigidbody.getInfo().buffer, b_rigidbody.getInfo().offset, rb);
				vk::BufferMemoryBarrier to_read[] = {
					b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_relative_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				world->m_rigitbody_desc_layout.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(world->m_context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descriptor_set = std::move(world->m_context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_rigidbody.getInfo(),
				b_relative_pos.getInfo(),
				b_rbparticle.getInfo(),
				b_rbpos_bit.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
			};
			world->m_context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}
	}

}

