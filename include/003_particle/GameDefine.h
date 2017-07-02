#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Loader.h>
#include <btrlib/cCamera.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/sGlobal.h>
#include <003_particle/CircleIndex.h>

struct ParticleInfo
{
	uint32_t m_max_num;
	uint32_t m_emit_max_num;
};
struct MapInfo
{
	glm::vec4 m_cell_size;
	glm::ivec4 m_cell_num;
};

struct Scene
{
	CircleIndex<uint32_t, 2> m_circle_index;
	btr::UpdateBuffer<CameraGPU> m_camera;
	MapInfo m_map_info_cpu;

	void setup(std::shared_ptr<btr::Loader>& loader)
	{
		m_map_info_cpu.m_cell_size = glm::vec4(10.f, 1.f, 10.f, 0.f);
		m_map_info_cpu.m_cell_num = glm::vec4(127, 127, 0, 0);

		btr::UpdateBufferDescriptor update_desc;
		update_desc.device_memory = loader->m_uniform_memory;
		update_desc.staging_memory = loader->m_staging_memory;
		update_desc.frame_max = sGlobal::FRAME_MAX;
		m_camera.setup(update_desc);
	}

	void execute(std::shared_ptr<btr::Executer>& executer)
	{
		m_circle_index++;

		vk::CommandBuffer cmd = executer->m_cmd;

		std::vector<vk::BufferMemoryBarrier> to_transfer = {
			m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, to_transfer, {});

		auto* camera = cCamera::sCamera::Order().getCameraList()[0];
		CameraGPU camera_GPU;
		camera_GPU.setup(*camera);
		m_camera.subupdate(camera_GPU);
		m_camera.update(cmd);

		std::vector<vk::BufferMemoryBarrier> to_draw_barrier = {
			m_camera.getAllocateMemory().makeMemoryBarrier(vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexShader, {}, {}, { to_draw_barrier }, {});

	}
};
extern std::unique_ptr<Scene> g_scene;