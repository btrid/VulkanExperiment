#pragma once

#include <array>
#include <memory>
#include <btrlib/BufferMemory.h>
#include <btrlib/cCamera.h>
#include <btrlib/Loader.h>

#include <applib/App.h>
#include <003_particle/sScene.h>
#include <003_particle/MazeGenerator.h>
#include <003_particle/Geometry.h>
#include <003_particle/CircleIndex.h>
#include <003_particle/cTriangleLL.h>
#include <003_particle/sBoid.h>

struct ParticleInfo
{
	uint32_t m_max_num;
	uint32_t m_emit_max_num;
};
struct ParticleData
{
	glm::vec4 m_pos;	//!< xyz:pos w:scale
	glm::vec4 m_vel;	//!< xyz:dir w:distance
	glm::ivec2 m_map_index;
	uint32_t _p;
	uint32_t _p1;

	uint32_t m_type;
	uint32_t m_flag;
	float m_life;
	uint32_t _pp;
};

struct cParticlePipeline
{
	struct Private 
	{
		enum : uint32_t {
			COMPUTE_UPDATE,
			COMPUTE_EMIT,
			COMPUTE_NUM,
		};
		enum : uint32_t {
			GRAPHICS_SHADER_VERTEX_PARTICLE,
			GRAPHICS_SHADER_FRAGMENT_PARTICLE,
			GRAPHICS_SHADER_NUM,
		};

		enum : uint32_t
		{
			PIPELINE_LAYOUT_UPDATE,
			COMPUTE_PIPELINE_LAYOUT_EMIT,
			GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW,
			GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW,
			PIPELINE_LAYOUT_NUM,
		};
		enum : uint32_t
		{
			DESCRIPTOR_UPDATE,
			DESCRIPTOR_EMIT,
			DESCRIPTOR_NUM,
		};

		btr::AllocatedMemory m_particle;
		btr::AllocatedMemory m_particle_info;
		btr::UpdateBuffer<std::array<ParticleData, 1024>> m_particle_emit;
		btr::AllocatedMemory m_particle_counter;

		btr::AllocatedMemory m_particle_draw_indiret_info;

		std::array<vk::DescriptorSetLayout, DESCRIPTOR_NUM> m_descriptor_set_layout;
		std::array<vk::DescriptorSet, DESCRIPTOR_NUM> m_descriptor_set;
		std::array<vk::PipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;

		std::vector<vk::Pipeline> m_compute_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, COMPUTE_NUM> m_compute_shader_info;

		std::vector<vk::Pipeline> m_graphics_pipeline;
		std::array<vk::PipelineShaderStageCreateInfo, GRAPHICS_SHADER_NUM> m_graphics_shader_info;

		ParticleInfo m_particle_info_cpu;



		void setup(btr::Loader& loader);

		void execute(btr::Executer& executer)
		{
			vk::CommandBuffer cmd = executer.m_cmd;
			{
				// transfer
				std::vector<vk::BufferMemoryBarrier> to_transfer = { 
					m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

				cmd.updateBuffer<vk::DrawIndirectCommand>(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, vk::DrawIndirectCommand(0, 1, 0, 0));


				std::vector<vk::BufferMemoryBarrier> to_update_barrier = {
					m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_update_barrier }, {});
			}

			uint src_offset = sGlobal::Order().getCPUIndex() == 1 ? (m_particle.getBufferInfo().range/sizeof(ParticleData) / 2) : 0;
			uint dst_offset = sGlobal::Order().getCPUIndex() == 0 ? (m_particle.getBufferInfo().range / sizeof(ParticleData) / 2) : 0;
			{
				// update
				struct UpdateConstantBlock
				{
					float m_deltatime;
					uint m_src_offset;
					uint m_dst_offset;
				};
				UpdateConstantBlock block;
				block.m_deltatime = sGlobal::Order().getDeltaTime();
				block.m_src_offset = src_offset;
				block.m_dst_offset = dst_offset;
				cmd.pushConstants<UpdateConstantBlock>(m_pipeline_layout[COMPUTE_UPDATE], vk::ShaderStageFlagBits::eCompute, 0, block);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline[COMPUTE_UPDATE]);
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE], 0, m_descriptor_set[DESCRIPTOR_UPDATE], {});
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_UPDATE], 1, sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP), {});
				auto groups = app::calcDipatchGroups(glm::uvec3(8192 / 2, 1, 1), glm::uvec3(1024, 1, 1));
				cmd.dispatch(groups.x, groups.y, groups.z);

			}


			{
				static int count;
				count++;
				count %= 3000;
				if (count == 1 )
				{
					auto particle_barrier = m_particle.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite);
					std::vector<vk::BufferMemoryBarrier> to_emit_barrier =
					{
						particle_barrier,
						m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_emit_barrier, {});

					// emit
					std::array<ParticleData, 200> data;
					for (auto& p : data)
					{
						p.m_pos = glm::vec4(std::rand()%20+50.f, 0.f, std::rand() % 20 + 50.f, 1.f);
						p.m_vel = glm::vec4(glm::normalize(glm::vec3(std::rand() % 50-25, 0.f, std::rand() % 50-25 + 0.5f)), std::rand()%50 + 15.5f);
						p.m_life = std::rand() % 50 + 240;

						p.m_map_index = sScene::Order().calcMapIndex(p.m_pos);
					}
					m_particle_emit.subupdate(data.data(), vector_sizeof(data), 0);

					auto to_transfer = m_particle_emit.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});
					m_particle_emit.update(cmd);
					auto to_read = m_particle_emit.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, to_read, {});

					struct EmitConstantBlock
					{
						uint m_emit_num;
						uint m_offset;
					};
					EmitConstantBlock block;
					block.m_emit_num = data.size();
					block.m_offset = dst_offset;
					cmd.pushConstants<EmitConstantBlock>(m_pipeline_layout[COMPUTE_EMIT], vk::ShaderStageFlagBits::eCompute, 0, block);
					cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_compute_pipeline[COMPUTE_EMIT]);
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_EMIT], 0, m_descriptor_set[DESCRIPTOR_UPDATE], {});
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[COMPUTE_PIPELINE_LAYOUT_EMIT], 1, m_descriptor_set[DESCRIPTOR_EMIT], {});
					auto groups = app::calcDipatchGroups(glm::uvec3(block.m_emit_num, 1, 1), glm::uvec3(1024, 1, 1));
					cmd.dispatch(groups.x, groups.y, groups.z);

				}

				struct DrawConstantBlock
				{
					uint m_src_offset;
				};
				DrawConstantBlock block;
				block.m_src_offset = dst_offset;
				cmd.pushConstants<DrawConstantBlock>(m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW], vk::ShaderStageFlagBits::eVertex, 0, block);
				auto particle_barrier = m_particle.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader, {}, {}, particle_barrier, {});

				auto to_draw = m_particle_counter.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { to_draw }, {});
			}

		}

		void draw(vk::CommandBuffer cmd)
		{
// 			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline[1]);
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_FLOOR_DRAW], 0, m_descriptor_set[DESCRIPTOR_DRAW_CAMERA], {});
// 			cmd.bindVertexBuffers(0, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().buffer }, { m_maze_geometry.m_resource->m_vertex.getBufferInfo().offset });
// 			cmd.bindIndexBuffer(m_maze_geometry.m_resource->m_index.getBufferInfo().buffer, m_maze_geometry.m_resource->m_index.getBufferInfo().offset, m_maze_geometry.m_resource->m_index_type);
// 			cmd.drawIndexedIndirect(m_maze_geometry.m_resource->m_indirect.getBufferInfo().buffer, m_maze_geometry.m_resource->m_indirect.getBufferInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

// 			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline[0]);
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW], 0, m_descriptor_set[DESCRIPTOR_UPDATE], {});
// 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[GRAPHICS_PIPELINE_LAYOUT_PARTICLE_DRAW], 1, m_descriptor_set[DESCRIPTOR_DRAW_CAMERA], {});
// 			cmd.drawIndirect(m_particle_counter.getBufferInfo().buffer, m_particle_counter.getBufferInfo().offset, 1, sizeof(vk::DrawIndirectCommand));
		}

	};
	std::unique_ptr<Private> m_private;

	void setup(btr::Loader& loader)
	{
		auto p = std::make_unique<Private>();
		p->setup(loader);

		m_private = std::move(p);
//		m_boid.setup(loader, *this);
	}

	void execute(btr::Executer& executer)
	{
		m_private->execute(executer);
//		m_boid.execute(executer);
	}

	void draw(vk::CommandBuffer cmd)
	{
//		m_boid.draw(cmd);
		m_private->draw(cmd);
	}
};

