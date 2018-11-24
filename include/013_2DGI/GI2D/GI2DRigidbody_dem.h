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

struct GI2DRigidbody_dem
{
	struct Rigidbody
	{
		int32_t pnum;
		int32_t solver_count;
		int32_t inertia;
		int32_t _p2;

		vec2 center;
		vec2 size;

		vec2 pos;
		vec2 vel;

		ivec2 pos_work;
		ivec2 vel_work;

		float angle;
		float angle_vel;
		int32_t angle_vel_work;
		float _pp1;

	};
	struct Constraint
	{
		int32_t r_id;
		float sinking;
		int32_t _p2;
		int32_t _p3;
		vec2 ri;
		vec2 rj;
		vec2 vi;
		vec2 vj;
	};
	enum
	{
		Particle_Num = 120,
	};
	enum Shader
	{
		Shader_Update,
		Shader_Pressure,
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
		Pipeline_Update,
		Pipeline_Pressure,
		Pipeline_Integrate,
		Pipeline_ToFragment,
		Pipeline_Num,
	};

	vec2 calcTangent(vec2 normal)
	{
		vec3 vec = vec3(0., 0., 1.);
		return normalize(cross(vec3(normal, 0.), vec)).xy;
	}
	void calc(Rigidbody rb, vec2 p, vec2& l, float& a)
	{
		float DT = 0.016f;
		vec2 local_pos = rotate(p, rb.angle);
		vec2 local_pos2 = rotate(p, rb.angle + rb.angle_vel*DT);
		vec2 pos = rb.pos + local_pos;

		vec2 vel = rb.vel + vec2(0., 9.8)*DT;
		vec2 angular_vel = cross(vec3(p, 0.), vec3(0., 0., rb.angle_vel*DT)).xy;
		vec2 angular_vel2 = cross(vec3(p, 0.), vec3(0., 0., rb.angle + rb.angle_vel*DT)).xy;
		vec2 angular_vel4 = cross(vec3(local_pos, 0.), vec3(0., 0., rb.angle_vel*DT)).xy;
		vec2 angular_vel3 = local_pos2-local_pos;

		vec2 vel_ = normalize(vel + angular_vel);
		float length = glm::length(vel + angular_vel);
	}

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
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

			}

			{
				const char* name[] =
				{
					"Rigid_Update.comp.spv",
					"Rigid_CalcPressure.comp.spv",
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
					gi2d_context->getDescriptorSetLayout(),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_Rigid] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}

			// pipeline
			{
				std::array<vk::PipelineShaderStageCreateInfo, Shader_Num> shader_info;
				shader_info[0].setModule(m_shader[Shader_Update].get());
				shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[0].setPName("main");
				shader_info[1].setModule(m_shader[Shader_Pressure].get());
				shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[1].setPName("main");
				shader_info[2].setModule(m_shader[Shader_Integrate].get());
				shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[2].setPName("main");
				shader_info[3].setModule(m_shader[Shader_ToFragment].get());
				shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[3].setPName("main");
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
				};
				auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
				m_pipeline[Pipeline_Update] = std::move(compute_pipeline[0]);
				m_pipeline[Pipeline_Pressure] = std::move(compute_pipeline[1]);
				m_pipeline[Pipeline_Integrate] = std::move(compute_pipeline[2]);
				m_pipeline[Pipeline_ToFragment] = std::move(compute_pipeline[3]);
			}

		}

		{
			{
				b_rigidbody = m_context->m_storage_memory.allocateMemory<Rigidbody>({ 1,{} });

				b_rbpos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });
				b_relative_pos = m_context->m_storage_memory.allocateMemory<vec2>({ Particle_Num,{} });

				{

					std::vector<vec2> pos(Particle_Num);
					std::vector<vec2> rela_pos(Particle_Num);
					vec2 center = vec2(0.f);
#if 1

					for (int y = 0; y < 4; y++) 
					{
						for (int x = 0; x < 30; x++)
						{
							pos[x + y * 30].x = 123.f + x;
							pos[x + y * 30].y = 220.f + y;
						}
					}
#else
					for (auto& p : pos) {
						p = glm::diskRand(12.f) + 150.f;
					}
#endif
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

					Rigidbody rb;
					rb.pos = center;
					rb.center = size/2.f;
					rb.size = size_max;
					rb.vel = vec2(0.f);
					rb.pos_work = ivec2(0.f);
					rb.vel_work = ivec2(0.f);
					rb.angle_vel_work = 0.;
					rb.pnum = Particle_Num;
					rb.angle = 0.f;
					rb.angle_vel = 0.f;
					rb.solver_count = 0;

					cmd.updateBuffer<Rigidbody>(b_rigidbody.getInfo().buffer, b_rigidbody.getInfo().offset, rb);

					vk::BufferMemoryBarrier to_read[] = {
						b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
						b_relative_pos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
						b_rbpos.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, {},
						0, nullptr, array_length(to_read), to_read, 0, nullptr);


					rb.vel = vec2(0.f, 9.8f);
// 					rb.angle = 1.1f;
// 					rb.angle_vel = 0.4f;

//					for(;;)
					{
						vec2 delta_linear_vel = vec2(0.);
						float delta_angular_vel = 0.;
						int count = 0;
						for (int i = 0; i < Particle_Num; i++)
						{
							vec2 p = rela_pos[i];
							vec2 local_pos = rotate(p, rb.angle);
							vec2 pos = rb.pos + local_pos;

							if (pos.y >= 240.f)
							{
								calc(rb, p, delta_linear_vel, delta_angular_vel);
								count++;

							}

						}
						delta_linear_vel /= (count !=0) ? count : 1;
//						delta_angular_vel /= (count != 0) ? count : 1;

						rb.vel += delta_linear_vel;
						rb.angle_vel += delta_angular_vel;

						rb.pos += rb.vel;
						rb.vel += vec2(0., 9.8)*0.016f;
						rb.angle += rb.angle_vel*0.016f;

					}

				}
				mat3 inertia_inv = inverse(mat3(vec3(1., 0., 0.), vec3(0., 1., 0.), vec3(0., 0., 1.)));

				auto angular_vel = cross(vec3(10.f, 2.f, 0.), vec3(0., 0., 31.14f));
				angular_vel /= angular_vel.z;
				int aa;
				aa = 0;


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
			// 位置の更新
// 			vk::BufferMemoryBarrier to_read[] = {
// 				b_rbacc.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
// 			};
// 			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
// 				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Update].get());

			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);

		}

		{
			// 圧力の更新
			vk::BufferMemoryBarrier to_read[] = {
				m_gi2d_context->b_grid_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_rbpos.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Pressure].get());
			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		{

			{
				// 剛体の更新
				vk::BufferMemoryBarrier to_read[] = {
					b_rigidbody.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
					0, nullptr, array_length(to_read), to_read, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Integrate].get());
				cmd.dispatch(1, 1, 1);
			}
		}

		{
			// fragment_dataに書き込む
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


	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;


};
