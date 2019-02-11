#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>


mat2 crossMatrix2(float a)
{
	return mat2(0.f, a, -a, 0.f);
}

// ÂÊvf@, £Uvf@ DEM
struct GI2DRigidbody_dem
{
	struct Rigidbody
	{
		int32_t pnum;
		int32_t solver_count;
		float inertia;
		int32_t _p2;

		vec2 center;
		vec2 size;

		vec2 pos;
		vec2 pos_old;
		vec2 vel;
		vec2 vel_old;

		ivec2 pos_work;
		ivec2 vel_work;

		float angle;
		float angle_vel;
		int32_t angle_vel_work;
		uint32_t dist;

		ivec2 pos_bit_size;
	};

	/// S©
	struct Constraint
	{
		vec2 axis; ///< S©²
		float jacDiagInv; ///< S©®Ìªê
		float rhs; ///< úS©Í

		float lowerLimit; ///< S©ÍÌºÀ
		float upperLimit; ///< S©ÍÌãÀ
		float accumImpulse; ///< ~Ï³êéS©Í
		float _p;
	};

	/// ÕËîñ
	struct Contact
	{
		float distance; ///< ÑÊ[x
		float friction; ///< C
		vec2 pointA; ///< ÕË_iÌAÌ[JÀWnj
		vec2 pointB; ///< ÕË_iÌBÌ[JÀWnj
		vec2 normal; ///< ÕË_Ì@üxNgi[hÀWnj
		Constraint constraints[2]; ///< S©
	};

	struct rbParticle
	{
//		uint32_t use_collision_detective;
		uint32_t contact_index;
		uint32_t is_contact;
	};

	enum
	{
		PX = 16,
		PY = 16,
		Particle_Num = PX*PY,
	};
	enum Shader
	{
		Shader_CollisionDetective,
		Shader_CollisionDetectiveBefore,
		Shader_CalcForce,
		Shader_Integrate,
		Shader_ToFragment,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_Rigid,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_CollisionDetective,
		Pipeline_CollisionDetectiveBefore,
		Pipeline_CalcForce,
		Pipeline_Integrate,
		Pipeline_ToFragment,
		Pipeline_Num,
	};

	GI2DRigidbody_dem(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		{

			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
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
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}

			{
				const char* name[] =
				{
					"Rigid_CollisionDetective.comp.spv",
					"Rigid_CollisionDetectiveBefore.comp.spv",
					"Rigid_CalcForce.comp.spv",
					"Rigid_Integrate.comp.spv",
					"Rigid_ToFragment.comp.spv",
				};
				static_assert(array_length(name) == Shader_Num, "not equal shader num");

				std::string path = btr::getResourceShaderPath();
				for (size_t i = 0; i < array_length(name); i++) {
					m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
				}
			}

			// pipeline layout
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_Rigid] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}

			// pipeline
			{
				std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
				shader_info[0].setModule(m_shader[Shader_CollisionDetective].get());
				shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[0].setPName("main");
				shader_info[1].setModule(m_shader[Shader_CollisionDetectiveBefore].get());
				shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[1].setPName("main");
				shader_info[2].setModule(m_shader[Shader_CalcForce].get());
				shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[2].setPName("main");
				shader_info[3].setModule(m_shader[Shader_Integrate].get());
				shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[3].setPName("main");
				shader_info[4].setModule(m_shader[Shader_ToFragment].get());
				shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[4].setPName("main");
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[0])
					.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[1])
					.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[2])
					.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[3])
					.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[4])
					.setLayout(m_pipeline_layout[PipelineLayout_Rigid].get()),
				};
				auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
				m_pipeline[Pipeline_CollisionDetective] = std::move(compute_pipeline[0]);
				m_pipeline[Pipeline_CollisionDetectiveBefore] = std::move(compute_pipeline[1]);
				m_pipeline[Pipeline_CalcForce] = std::move(compute_pipeline[2]);
				m_pipeline[Pipeline_Integrate] = std::move(compute_pipeline[3]);
				m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[4]);
			}

		}

		{
			{
				b_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ 1,{} });

				b_rbpos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
				b_relative_pos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
				b_rbparticle = m_context->m_storage_memory.allocateMemory<rbParticle>({ Particle_Num,{} });
				b_rbpos_bit = m_context->m_storage_memory.allocateMemory<uint64_t>({ 4*4,{} });
				b_rbcontact = m_context->m_storage_memory.allocateMemory<Contact>({ Particle_Num,{} });
				{

					std::vector<vec2> pos(Particle_Num);
					std::vector<vec2> rela_pos(Particle_Num);
					std::vector<rbParticle> pstate(Particle_Num);
					std::vector<uint64_t> bit(4*4);
					vec2 center = vec2(0.f);

					uint32_t contact_index = 0;
					for (int y = 0; y < PY; y++) 
					{
						for (int x = 0; x < PX; x++)
						{
							pos[x + y * PX].x = 123.f + x;
							pos[x + y * PX].y = 220.f + y;

							if (y == 0 || y == PY-1 || x == 0 || x== PX-1)
							{
								pstate[x + y * PX].contact_index = contact_index++;
							}
							else
							{
								pstate[x + y * PX].contact_index = -1;
							}
							pstate[x + y * PX].is_contact = 0;
							bit[x / 8 + y / 8 * 4] |= 1ull << (x % 8 + (y % 8) * 8);
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
					cmd.updateBuffer<vec2>(b_rbpos.getInfo().buffer, b_rbpos.getInfo().offset, pos);
					cmd.updateBuffer<vec2>(b_relative_pos.getInfo().buffer, b_relative_pos.getInfo().offset, rela_pos);
					cmd.updateBuffer<rbParticle>(b_rbparticle.getInfo().buffer, b_rbparticle.getInfo().offset, pstate);

					float inertia = 0.f;
					for (int32_t i = 0; i < rela_pos.size(); i++)
					{
//						vec2 r = pos[i] - center;
						inertia += dot(rela_pos[i], rela_pos[i]) /** mass*/;
					}

					Rigidbody rb;
					rb.pos = center;
					rb.pos_old = rb.pos;
					rb.center = size / 2.f;
					rb.inertia = inertia;
					rb.size = size_max;
					rb.vel = vec2(0.f);
					rb.pos_work = ivec2(0.f);
					rb.vel_work = ivec2(0.f);
					rb.angle_vel_work = 0.;
					rb.pnum = Particle_Num;
					rb.angle = 3.14f/4.f + 0.2;
					rb.angle_vel = 0.f;
					rb.solver_count = 0;
					rb.dist = -1;

					auto _p = glm::rotate(rela_pos[0], 3.14f / 4.f);
					vec3 delta_angular_vel_ = cross(vec3(5.f, 5.f, 0.), vec3(0.f, 10.f, 0.));
					vec3 delta_angular_vel1 = cross(vec3(-5.f, -5.f, 0.), vec3(0.f, 10.f, 0.));
					float delta_angular_vel = delta_angular_vel_.z;
					delta_angular_vel /= rb.inertia;

					cmd.updateBuffer<Rigidbody>(b_rigidbody.getInfo().buffer, b_rigidbody.getInfo().offset, rb);

					vk::BufferMemoryBarrier to_read[] = {
						b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
						b_relative_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
						b_rbpos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {},
						0, nullptr, array_length(to_read), to_read, 0, nullptr);
				}
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

				vk::DescriptorBufferInfo storages[] = {
					b_rigidbody.getInfo(),
					b_relative_pos.getInfo(),
					b_rbpos.getInfo(),
					b_rbparticle.getInfo(),
					b_rbpos_bit.getInfo(),
					b_rbcontact.getInfo(),
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
				context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}

	}
	void execute(vk::CommandBuffer cmd)
	{

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 0, m_descriptor_set.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Rigid].get(), 1, m_gi2d_context->getDescriptorSet(), {});


		{
			// XVOvZ
			vk::BufferMemoryBarrier to_read[] = {
				m_gi2d_context->b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_rbpos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CollisionDetectiveBefore].get());
			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
//			cmd.dispatch(num.x, num.y, num.z);
		}

		{
			// ÕË»è
			vk::BufferMemoryBarrier to_read[] = {
				b_rbparticle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),

			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CollisionDetective].get());

			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}
		{
			// ÕËvZ
			vk::BufferMemoryBarrier to_read[] = {
				b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_CalcForce].get());

			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		{
			// ÌÌXV
			vk::BufferMemoryBarrier to_read[] = {
				b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Integrate].get());
			cmd.dispatch(1, 1, 1);
		}

		{
			// fragment_dataÉ«Þ
			vk::BufferMemoryBarrier to_read[] = {
				m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_ToFragment].get());
			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		vk::BufferMemoryBarrier to_read[] = {
			m_gi2d_context->b_fragment.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eComputeShader, {},
			0, nullptr, array_length(to_read), to_read, 0, nullptr);
	}

	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<Rigidbody> b_rigidbody;
	btr::BufferMemoryEx<vec2> b_relative_pos;
	btr::BufferMemoryEx<vec2> b_rbpos;
	btr::BufferMemoryEx<rbParticle> b_rbparticle;
	btr::BufferMemoryEx<uint64_t> b_rbpos_bit;
	btr::BufferMemoryEx<Contact> b_rbcontact;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;


};
