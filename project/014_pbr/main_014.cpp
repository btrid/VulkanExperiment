#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>


#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct Model
{
	struct Info
	{
		vec4 m_aabb_min;
		vec4 m_aabb_max;
		uint m_vertex_num;
		uint m_primitive_num;
		uvec3 m_voxel_reso;
	};
	btr::BufferMemory b_vertex;
	btr::BufferMemory b_normal;
	btr::BufferMemory b_index;
	btr::BufferMemoryEx<Info> u_info;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> b_draw_cmd;

	vk::UniqueDescriptorSet m_DS_Model;

	Info m_info;

	static std::shared_ptr<Model> LoadModel(btr::Context& ctx, const std::string& filename)
	{
		int OREORE_PRESET = 0
			| aiProcess_JoinIdenticalVertices
			| aiProcess_ImproveCacheLocality
			| aiProcess_LimitBoneWeights
			| aiProcess_RemoveRedundantMaterials
			| aiProcess_SplitLargeMeshes
			| aiProcess_SortByPType
			| aiProcess_Triangulate
			;
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(filename, OREORE_PRESET);
		if (!scene) { return nullptr; }

		unsigned numIndex = 0;
		unsigned numVertex = 0;
		for (size_t i = 0; i < scene->mNumMeshes; i++)
		{
			numVertex += scene->mMeshes[i]->mNumVertices;
			numIndex += scene->mMeshes[i]->mNumFaces * 3;
		}

		auto vertex = ctx.m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
		auto normal = ctx.m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
		auto index = ctx.m_staging_memory.allocateMemory(numIndex * sizeof(uint32_t), true);

		uint32_t index_offset = 0;
		uint32_t vertex_offset = 0;
		Info info{};

		for (size_t i = 0; i < scene->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[i];
			for (u32 n = 0; n < mesh->mNumFaces; n++)
			{
				std::copy(mesh->mFaces[n].mIndices, mesh->mFaces[n].mIndices + 3, index.getMappedPtr<uint32_t>(index_offset));
				index_offset += 3;
			}

			for (uint32_t v = 0; v < mesh->mNumVertices; v++)
			{
//				mesh->mVertices[v] += aiVector3D(1.f);
//				mesh->mVertices[v] *= param.scale;
				info.m_aabb_min.x = std::min(info.m_aabb_min.x, mesh->mVertices[v].x);
				info.m_aabb_min.y = std::min(info.m_aabb_min.y, mesh->mVertices[v].y);
				info.m_aabb_min.z = std::min(info.m_aabb_min.z, mesh->mVertices[v].z);
				info.m_aabb_max.x = std::max(info.m_aabb_max.x, mesh->mVertices[v].x);
				info.m_aabb_max.y = std::max(info.m_aabb_max.y, mesh->mVertices[v].y);
				info.m_aabb_max.z = std::max(info.m_aabb_max.z, mesh->mVertices[v].z);
			}

			std::copy(mesh->mVertices, mesh->mVertices + mesh->mNumVertices, vertex.getMappedPtr<aiVector3D>(vertex_offset));
			std::copy(mesh->mNormals, mesh->mNormals + mesh->mNumVertices, normal.getMappedPtr<aiVector3D>(vertex_offset));


			vertex_offset += mesh->mNumVertices;
		}
		importer.FreeScene();

		info.m_vertex_num = numVertex;
		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);
		std::shared_ptr<Model> model = std::make_shared<Model>();
		{
			model->b_vertex = ctx.m_vertex_memory.allocateMemory(numVertex * sizeof(aiVector3D));
			model->b_normal = ctx.m_vertex_memory.allocateMemory(numVertex * sizeof(aiVector3D));
			model->b_index = ctx.m_vertex_memory.allocateMemory(numIndex * sizeof(uint32_t));

			std::array<vk::BufferCopy, 3> copy;
			copy[0].srcOffset = vertex.getInfo().offset;
			copy[0].dstOffset = model->b_vertex.getInfo().offset;
			copy[0].size = vertex.getInfo().range;
			copy[1].srcOffset = normal.getInfo().offset;
			copy[1].dstOffset = model->b_normal.getInfo().offset;
			copy[1].size = normal.getInfo().range;
			copy[2].srcOffset = index.getInfo().offset;
			copy[2].dstOffset = model->b_index.getInfo().offset;
			copy[2].size = index.getInfo().range;
			cmd.copyBuffer(vertex.getInfo().buffer, model->b_vertex.getInfo().buffer, copy);
		}

		{
			info.m_primitive_num = numIndex / 3;
			model->m_info = info;
			model->u_info = ctx.m_uniform_memory.allocateMemory<Info>(1);

			cmd.updateBuffer<Info>(model->u_info.getInfo().buffer, model->u_info.getInfo().offset, info);

			model->b_draw_cmd = ctx.m_storage_memory.allocateMemory<vk::DrawIndirectCommand>(1);
			cmd.updateBuffer<vk::DrawIndirectCommand>(model->b_draw_cmd.getInfo().buffer, model->b_draw_cmd.getInfo().offset, vk::DrawIndirectCommand(numIndex, 1, 0, 0));
		}


		vk::BufferMemoryBarrier barrier[] = {
			model->b_vertex.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->b_normal.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->b_index.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->u_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, {}, { array_size(barrier), barrier }, {});

		return model;
	}

};


int main()
{
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	app.setup();

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_render_target(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			std::vector<vk::CommandBuffer> cmds =
			{
				clear_render_target.execute(),
				present_pipeline.execute(),
			};
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

