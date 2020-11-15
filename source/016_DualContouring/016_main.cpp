#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
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

#include <applib/sAppImGui.h>


#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>
struct Model
{
	btr::BufferMemory m_vertex;
	btr::BufferMemory m_index;
};

std::shared_ptr<Model> LoadModel(const std::shared_ptr<btr::Context>& context, const std::string& filename)
{
	int OREORE_PRESET = 0
		| aiProcess_JoinIdenticalVertices
		| aiProcess_ImproveCacheLocality
		| aiProcess_LimitBoneWeights
		| aiProcess_RemoveRedundantMaterials
		| aiProcess_SplitLargeMeshes
		| aiProcess_SortByPType
		//		| aiProcess_OptimizeMeshes
		| aiProcess_Triangulate
		//		| aiProcess_MakeLeftHanded
		;
	cStopWatch timer;
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
//	std::vector<uint32_t> index;
//	std::vector<aiVector3D> vertex;
//	index.reserve(numIndex);
//	vertex.reserve(numVertex);

	auto vertex = context->m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
	auto index = context->m_staging_memory.allocateMemory(numIndex * sizeof(uint32_t), true);

	uint32_t index_offset = 0;
	uint32_t vertex_offset = 0;
	for (size_t i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[i];
		for (u32 n = 0; n < mesh->mNumFaces; n++) {
//			std::copy(mesh->mFaces[n].mIndices, mesh->mFaces[n].mIndices + 3, std::back_inserter(index));
			std::copy(mesh->mFaces[n].mIndices, mesh->mFaces[n].mIndices + 3, vertex.getMappedPtr<uint32_t>(index_offset));
			index_offset += 3;
		}
//		std::copy(mesh->mVertices, mesh->mVertices + mesh->mNumVertices, std::back_inserter(vertex));
		std::copy(mesh->mVertices, mesh->mVertices + mesh->mNumVertices, vertex.getMappedPtr<aiVector3D>(vertex_offset));
		vertex_offset += mesh->mNumVertices;
	}

	importer.FreeScene();

	std::shared_ptr<Model> model = std::make_shared<Model>();
	model->m_vertex = context->m_vertex_memory.allocateMemory(numVertex*sizeof(aiVector3D));
	model->m_index = context->m_vertex_memory.allocateMemory(numIndex*sizeof(uint32_t));

	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
	std::array<vk::BufferCopy, 2> copy;
	copy[0].srcOffset = vertex.getInfo().offset;
	copy[0].dstOffset = model->m_vertex.getInfo().offset;
	copy[0].size = vertex.getInfo().range;
	copy[1].srcOffset = index.getInfo().offset;
	copy[1].dstOffset = model->m_index.getInfo().offset;
	copy[1].size = index.getInfo().range;
	cmd.copyBuffer(vertex.getInfo().buffer, model->m_vertex.getInfo().buffer, copy);


	return model;
}
int main()
{

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());



	auto model = LoadModel(context, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\tiny.x");
	{

	}

	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}
}