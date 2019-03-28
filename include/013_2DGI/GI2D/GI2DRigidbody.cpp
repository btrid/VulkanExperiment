#pragma once
#include "GI2DRigidbody.h"
#include "GI2DPhysicsWorld.h"

GI2DRigidbody::GI2DRigidbody(const std::shared_ptr<PhysicsWorld>& world, const uvec4& box)
{
	auto cmd = world->m_context->m_cmd_pool->allocCmdTempolary(0);

	m_particle_num = box.z * box.w;

	{
		{
			b_rigidbody = world->m_context->m_storage_memory.allocateMemory<Rigidbody>({ 1,{} });
			b_rbparticle = world->m_context->m_storage_memory.allocateMemory<rbParticle>({ m_particle_num,{} });

			{

				std::vector<vec2> pos(m_particle_num);
				std::vector<rbParticle> pstate(m_particle_num);
				std::vector<uint64_t> bit(4 * 4);
				vec2 center = vec2(0.f);

				uint32_t contact_index = 0;
				for (int y = 0; y < box.w; y++)
				{
					for (int x = 0; x < box.z; x++)
					{
						pos[x + y * box.z].x = box.x + x;
						pos[x + y * box.z].y = box.y + y;

						if (y == 0 || y == box.w - 1 || x == 0 || x == box.z - 1)
						{
							pstate[x + y * box.z].contact_index = contact_index++;
						}
						else
						{
							pstate[x + y * box.z].contact_index = -1;
						}
						pstate[x + y * box.z].is_contact = 0;
					}
				}
				for (auto& p : pos) {
					center += p;
				}
				center /= pos.size();

				vec2 size = vec2(0.f);
				vec2 size_max = vec2(0.f);
				for (int32_t i = 0; i < pstate.size(); i++)
				{
					pstate[i].relative_pos = pos[i] - center;
					size += pstate[i].relative_pos;
					size_max = glm::max(abs(pstate[i].relative_pos), size_max);
				}

				size /= pos.size();
				cmd.updateBuffer<rbParticle>(b_rbparticle.getInfo().buffer, b_rbparticle.getInfo().offset, pstate);

				float inertia = 0.f;
				for (int32_t i = 0; i < pstate.size(); i++)
				{
					//						if (pstate[i].contact_index >= 0)
					{
						inertia += dot(pstate[i].relative_pos, pstate[i].relative_pos) /** mass*/;
					}
				}
				inertia /= 12.f;
				Rigidbody rb;
				static uint32_t s_id;
				rb.id = ++s_id;
				rb.pos = center;
				rb.pos_old = rb.pos;
				rb.center = size / 2.f;
				rb.inertia = inertia;
				rb.size = size_max;
				rb.vel = vec2(0.f);
				rb.pos_work = ivec2(0);
				rb.vel_work = ivec2(0);
				rb.angle_vel_work = 0;
				rb.pnum = m_particle_num;
				rb.angle = 3.14f / 4.f + 0.2f;
//					rb.angle = 0.f;
				rb.angle_vel = 0.f;
				rb.solver_count = 0;
				rb.dist = -1;
				rb.damping_work = ivec2(0);

				cmd.updateBuffer<Rigidbody>(b_rigidbody.getInfo().buffer, b_rigidbody.getInfo().offset, rb);
				vk::BufferMemoryBarrier to_read[] = {
					b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);
			}
		}
		{
			vk::DescriptorSetLayout layouts[] = {
				world->m_rigidbody_desc_layout.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(world->m_context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descriptor_set = std::move(world->m_context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo storages[] = {
				b_rigidbody.getInfo(),
				b_rbparticle.getInfo(),
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

