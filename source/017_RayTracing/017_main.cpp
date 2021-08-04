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
#include <applib/sCameraManager.h>

#include <btrlib/Context.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#include <017_Raytracing/voxel1.h>
#include <017_Raytracing/voxel2.h>


struct Context
{
	enum DSL
	{
		DSL_Model,
		DSL_DC,
		DSL_Worker,
		DSL_Num,
	};

	std::shared_ptr<btr::Context> m_ctx;

	vk::UniqueDescriptorSetLayout m_DSL[DSL_Num];

	Context(std::shared_ptr<btr::Context>& ctx)
	{
		m_ctx = ctx;
		auto cmd = ctx->m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Model] = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}

		}

	}
};
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

	static std::shared_ptr<Model> LoadModel(Context& ctx, const std::string& filename)
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
		if (!scene) { assert(false); return nullptr; }

		unsigned numIndex = 0;
		unsigned numVertex = 0;
		for (size_t i = 0; i < scene->mNumMeshes; i++)
		{
			numVertex += scene->mMeshes[i]->mNumVertices;
			numIndex += scene->mMeshes[i]->mNumFaces * 3;
		}


		auto vertex = ctx.m_ctx->m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
		auto normal = ctx.m_ctx->m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
		auto index = ctx.m_ctx->m_staging_memory.allocateMemory(numIndex * sizeof(uint32_t), true);

		uint32_t index_offset = 0;
		uint32_t vertex_offset = 0;
		Info info{};
		info.m_aabb_min = vec4{ 999.f };
		info.m_aabb_max = vec4{ -999.f };
		info.m_voxel_reso = uvec3(256);
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
 				mesh->mVertices[v] += aiVector3D(100.f, 0.f, 100.f);
 				mesh->mVertices[v].x *= 3.f;
 				mesh->mVertices[v].y *= 3.f;
 				mesh->mVertices[v].z *= 3.f;
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
		auto cmd = ctx.m_ctx->m_cmd_pool->allocCmdTempolary(0);
		std::shared_ptr<Model> model = std::make_shared<Model>();
		{
			model->b_vertex = ctx.m_ctx->m_vertex_memory.allocateMemory(numVertex * sizeof(aiVector3D));
			model->b_normal = ctx.m_ctx->m_vertex_memory.allocateMemory(numVertex * sizeof(aiVector3D));
			model->b_index = ctx.m_ctx->m_vertex_memory.allocateMemory(numIndex * sizeof(uint32_t));

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
			model->u_info = ctx.m_ctx->m_uniform_memory.allocateMemory<Info>(1);

			cmd.updateBuffer<Info>(model->u_info.getInfo().buffer, model->u_info.getInfo().offset, info);

			model->b_draw_cmd = ctx.m_ctx->m_storage_memory.allocateMemory<vk::DrawIndirectCommand>(1);
			cmd.updateBuffer<vk::DrawIndirectCommand>(model->b_draw_cmd.getInfo().buffer, model->b_draw_cmd.getInfo().offset, vk::DrawIndirectCommand(numIndex, 1, 0, 0));
		}


		vk::BufferMemoryBarrier barrier[] = {
			model->b_vertex.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->b_normal.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->b_index.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->u_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, {}, { array_size(barrier), barrier }, {});


		{
			vk::DescriptorSetLayout layouts[] =
			{
				ctx.m_DSL[Context::DSL_Model].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			model->m_DS_Model = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] =
			{
				model->u_info.getInfo(),
			};
			vk::DescriptorBufferInfo model_storages[] =
			{
				model->b_vertex.getInfo(),
				model->b_normal.getInfo(),
				model->b_index.getInfo(),
				model->b_draw_cmd.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(model->m_DS_Model.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(model_storages))
				.setPBufferInfo(model_storages)
				.setDstBinding(1)
				.setDstSet(model->m_DS_Model.get()),
			};
			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}

		return model;
	}

};

struct Voxel_With_Model
{
	enum 
	{
		VoxelReso = 512,
	};
	enum
	{
		DSL_Voxel,
		DSL_Num,
	};
	enum
	{
		DS_Voxel,
		DS_Num,
	};

	enum
	{
		Pipeline_MakeVoxel,
		Pipeline_MakeVoxelTopChild,
		Pipeline_MakeVoxelMidChild,
		Pipeline_MakeHashMapMask,

		Pipeline_RenderVoxel,
		Pipeline_Debug_RenderVoxel,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_MakeVoxel,
		PipelineLayout_RenderVoxel,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	vk::UniqueRenderPass m_RP_debugvoxel;
	vk::UniqueFramebuffer m_FB_debugvoxel;

	vk::UniqueRenderPass m_RP_makevoxel;
	vk::UniqueFramebuffer m_FB_makevoxel;


	struct VoxelInfo
	{
		uvec4 reso;
	};


	struct InteriorNode
	{
		uvec2 bitmask;
		uint child;
	};
	struct LeafNode
	{
		uint16_t normal;
		uint albedo;
	};

	btr::BufferMemoryEx<VoxelInfo> u_info;
	btr::BufferMemoryEx<int> b_hashmap;
	btr::BufferMemoryEx<uvec2> b_hashmap_mask;
	btr::BufferMemoryEx<uvec4> b_interior_counter;
	btr::BufferMemoryEx<uint> b_leaf_counter;
	btr::BufferMemoryEx<InteriorNode> b_interior;
	btr::BufferMemoryEx<LeafNode> b_leaf;
	btr::BufferMemoryEx<mat4> u_pvmat;

	VoxelInfo m_info;

	Voxel_With_Model(Context& ctx, RenderTarget& rt)
	{
		auto cmd = ctx.m_ctx->m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eUniformBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL[DSL_Voxel] = ctx.m_ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		// descriptor set
		{
			m_info.reso = uvec4(VoxelReso, VoxelReso, VoxelReso, 1);
			uvec4 hash_reso = m_info.reso >> 2u >> 2u;
			uint num = m_info.reso.x * m_info.reso.y * m_info.reso.z;
			u_info = ctx.m_ctx->m_uniform_memory.allocateMemory<VoxelInfo>(1);
			b_hashmap = ctx.m_ctx->m_storage_memory.allocateMemory<int>(num / 64 / 64);
			b_hashmap_mask = ctx.m_ctx->m_storage_memory.allocateMemory<uvec2>(num / 64 / 64 / 64);
			b_interior_counter = ctx.m_ctx->m_storage_memory.allocateMemory<uvec4>(3);
			b_leaf_counter = ctx.m_ctx->m_storage_memory.allocateMemory<uint>(1);
			b_interior = ctx.m_ctx->m_storage_memory.allocateMemory<InteriorNode>(num / 64 / 4);
			b_leaf = ctx.m_ctx->m_storage_memory.allocateMemory<LeafNode>(num / 64 / 4);

			u_pvmat = ctx.m_ctx->m_uniform_memory.allocateMemory<mat4>(3);
			cmd.updateBuffer<VoxelInfo>(u_info.getInfo().buffer, u_info.getInfo().offset, m_info);

			mat4 pv[3];
			pv[0] = glm::ortho(-1024.f, 1024.f, -256.f, 256.f, 0.f, 2048.f) * glm::lookAt(vec3(0, 256, 1024), vec3(0, 256, 1024) + vec3(2048, 0, 0), vec3(0.f, 1.f, 0.f));
			pv[0] = glm::ortho(0.f, (float)VoxelReso, 0.f, (float)VoxelReso, 0.f, (float)VoxelReso);// * glm::lookAt(vec3(0, 256, 256), vec3(0, 256, 256) + vec3(512, 0, 0), vec3(0.f, 1.f, 0.f));
			pv[1] = glm::ortho(-1024.f, 1024.f, -1024.f, 1024.f, 0.f, 512.f) * glm::lookAt(vec3(1024, 0, 1024), vec3(1024, 0, 1024) + vec3(0, 512, 0), vec3(0.f, 0.f, -1.f));
			pv[2] = glm::ortho(-1024.f, 1024.f, -256.f, 256.f, 0.f, 2048.f) * glm::lookAt(vec3(1024, 256, 0), vec3(1024, 256, 0) + vec3(0, 0, 2048), vec3(0.f, 1.f, 0.f));
			cmd.updateBuffer<mat4[3]>(u_pvmat.getInfo().buffer, u_pvmat.getInfo().offset, pv);

			auto a = pv[0] * vec4(100.f, 100.f, 100.f, 1.f);

			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DSL_Voxel].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[DS_Voxel] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] =
			{
				u_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] =
			{
				b_hashmap.getInfo(),
				b_hashmap_mask.getInfo(),
				b_interior_counter.getInfo(),
				b_leaf_counter.getInfo(),
				b_interior.getInfo(),
				b_leaf.getInfo(),
			};
			vk::DescriptorBufferInfo uniforms2[] =
			{
				u_pvmat.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(m_DS[DS_Voxel].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(1)
				.setDstSet(m_DS[DS_Voxel].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms2))
				.setPBufferInfo(uniforms2)
				.setDstBinding(10)
				.setDstSet(m_DS[DS_Voxel].get()),
			};
			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DSL_Voxel].get(),
				};
				vk::PushConstantRange ranges[] =
				{
					vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eFragment).setSize(4)
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_PL[PipelineLayout_MakeVoxel] = ctx.m_ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DSL_Voxel].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_RenderVoxel] = ctx.m_ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Voxel_AllocateTopChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_AllocateMidChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_MakeHashMapMask.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_Rendering.comp.spv", vk::ShaderStageFlagBits::eCompute},
			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			vk::ComputePipelineCreateInfo compute_pipeline_info[] =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[0])
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[1])
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[2])
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[3])
				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
			};
			m_pipeline[Pipeline_MakeVoxelTopChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_MakeVoxelMidChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
			m_pipeline[Pipeline_MakeHashMapMask] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
			m_pipeline[Pipeline_RenderVoxel] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
		}

		// graphics pipeline render
		{

			// レンダーパス
			{
				// sub pass
				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);

				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);

				m_RP_makevoxel = ctx.m_ctx->m_device.createRenderPassUnique(renderpass_info);

				{
					vk::FramebufferCreateInfo framebuffer_info;
					framebuffer_info.setRenderPass(m_RP_makevoxel.get());
					framebuffer_info.setWidth(VoxelReso);
					framebuffer_info.setHeight(VoxelReso);
					framebuffer_info.setLayers(1);

					m_FB_makevoxel = ctx.m_ctx->m_device.createFramebufferUnique(framebuffer_info);
				}
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"ModelVoxelize.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"ModelVoxelize.geom.spv", vk::ShaderStageFlagBits::eGeometry},
				{"ModelVoxelize.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport[] =
			{
				vk::Viewport(0.f, 0.f, VoxelReso, VoxelReso, 0.f, 1.f),
//				vk::Viewport(0.f, 0.f, (float)2048, (float)2048, 0.f, 1.f),
//				vk::Viewport(0.f, 0.f, (float)2048, (float)512, 0.f, 1.f),
			};
			vk::Rect2D scissor[] =
			{
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(VoxelReso, VoxelReso)),
//				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(2048, 2048)),
//				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(2048, 512)),
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			auto prop = ctx.m_ctx->m_physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceConservativeRasterizationPropertiesEXT>();
			auto& conservativeRasterProps= prop.get<vk::PhysicalDeviceConservativeRasterizationPropertiesEXT>();
			vk::PipelineRasterizationConservativeStateCreateInfoEXT conservativeRasterState_CI;
			conservativeRasterState_CI.conservativeRasterizationMode = vk::ConservativeRasterizationModeEXT::eOverestimate;
			conservativeRasterState_CI.extraPrimitiveOverestimationSize = conservativeRasterProps.maxExtraPrimitiveOverestimationSize;
			rasterization_info.pNext = &conservativeRasterState_CI;

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);


			vk::PipelineColorBlendStateCreateInfo blend_info;
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::VertexInputAttributeDescription vi_attrib;
			vi_attrib.binding = 0;
			vi_attrib.format = vk::Format::eR32G32B32Sfloat;
			vi_attrib.location = 0;
			vi_attrib.offset = 0;
			vk::VertexInputBindingDescription vi_binding;
			vi_binding.binding = 0;
			vi_binding.inputRate = vk::VertexInputRate::eVertex;
			vi_binding.stride = 12;
			vertex_input_info.vertexAttributeDescriptionCount = 1;
			vertex_input_info.pVertexAttributeDescriptions = &vi_attrib;
			vertex_input_info.vertexBindingDescriptionCount = 1;
			vertex_input_info.pVertexBindingDescriptions = &vi_binding;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get())
				.setRenderPass(m_RP_makevoxel.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline[Pipeline_MakeVoxel] = ctx.m_ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}

		// graphics pipeline debug

		{

			// レンダーパス
			{
				vk::AttachmentReference color_ref[] = {
					vk::AttachmentReference().setLayout(vk::ImageLayout::eColorAttachmentOptimal).setAttachment(0),
				};
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(1);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				// sub pass
				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(array_length(color_ref));
				subpass.setPColorAttachments(color_ref);
				subpass.setPDepthStencilAttachment(&depth_ref);

				vk::AttachmentDescription attach_desc[] =
				{
					// color1
					vk::AttachmentDescription()
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					vk::AttachmentDescription()
					.setFormat(rt.m_depth_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(array_length(attach_desc));
				renderpass_info.setPAttachments(attach_desc);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);

				m_RP_debugvoxel = ctx.m_ctx->m_device.createRenderPassUnique(renderpass_info);

				{
					vk::ImageView view[] = {
						rt.m_view,
						rt.m_depth_view,
					};
					vk::FramebufferCreateInfo framebuffer_info;
					framebuffer_info.setRenderPass(m_RP_debugvoxel.get());
					framebuffer_info.setAttachmentCount(array_length(view));
					framebuffer_info.setPAttachments(view);
					framebuffer_info.setWidth(rt.m_info.extent.width);
					framebuffer_info.setHeight(rt.m_info.extent.height);
					framebuffer_info.setLayers(1);

					m_FB_debugvoxel = ctx.m_ctx->m_device.createFramebufferUnique(framebuffer_info);
				}
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"VoxelDebug_Render.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"VoxelDebug_Render.geom.spv", vk::ShaderStageFlagBits::eGeometry},
				{"VoxelDebug_Render.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt.m_resolution.width, (float)rt.m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_resolution.width, rt.m_resolution.height));
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount(1);
			viewportInfo.setPScissors(&scissor);


			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);


			vk::PipelineColorBlendAttachmentState blend_state;
			blend_state.setBlendEnable(VK_FALSE);
			blend_state.setColorBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcColorBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstColorBlendFactor(vk::BlendFactor::eZero);
			blend_state.setAlphaBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
			blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
				| vk::ColorComponentFlagBits::eA);

			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(1);
			blend_info.setPAttachments(&blend_state);

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL[PipelineLayout_RenderVoxel].get())
				.setRenderPass(m_RP_debugvoxel.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline[Pipeline_Debug_RenderVoxel] = ctx.m_ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}
	}

	void execute_MakeVoxel(vk::CommandBuffer cmd, Model& model)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		_label.insert("Clear Voxel");
		{

			{
				vk::BufferMemoryBarrier write[] =
				{
					b_hashmap.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					b_leaf_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_size(write), write }, {});
			}

			{
				cmd.fillBuffer(b_hashmap.getInfo().buffer, b_hashmap.getInfo().offset, b_hashmap.getInfo().range, -1);
				cmd.updateBuffer<ivec4>(b_interior_counter.getInfo().buffer, b_interior_counter.getInfo().offset, { ivec4{0,1,1,0}, ivec4{0,1,1,0}, ivec4{0,1,1,0} });
				cmd.fillBuffer(b_leaf_counter.getInfo().buffer, b_leaf_counter.getInfo().offset, b_leaf_counter.getInfo().range, 0);
			}

		}

		_label.insert("Make Hash");
		{
			{
				vk::BufferMemoryBarrier read[] =
				{
					b_hashmap.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_leaf_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, { array_size(read), read }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_MakeVoxel].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
			cmd.pushConstants<int>(m_PL[PipelineLayout_MakeVoxel].get(), vk::ShaderStageFlagBits::eFragment, 0, 0);

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_RP_makevoxel.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_info.reso.x, m_info.reso.z)));
			begin_render_Info.setFramebuffer(m_FB_makevoxel.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindIndexBuffer(model.b_index.getInfo().buffer, model.b_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.bindVertexBuffers(0, model.b_vertex.getInfo().buffer, model.b_vertex.getInfo().offset);
			cmd.drawIndexed(model.m_info.m_primitive_num * 3, 1, 0, 0, 0);

			cmd.endRenderPass();

		}
		_label.insert("Make Top");
		{
			{
				vk::BufferMemoryBarrier read[] =
				{
					b_hashmap.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					b_interior.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, { array_size(read), read }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_MakeVoxel].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
			cmd.pushConstants<int>(m_PL[PipelineLayout_MakeVoxel].get(), vk::ShaderStageFlagBits::eFragment, 0, 1);

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_RP_makevoxel.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_info.reso.x, m_info.reso.z)));
			begin_render_Info.setFramebuffer(m_FB_makevoxel.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindIndexBuffer(model.b_index.getInfo().buffer, model.b_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.bindVertexBuffers(0, model.b_vertex.getInfo().buffer, model.b_vertex.getInfo().offset);
			cmd.drawIndexed(model.m_info.m_primitive_num * 3, 1, 0, 0, 0);

			cmd.endRenderPass();

		}


		_label.insert("Alloc Top Child");
		{
			{
				vk::BufferMemoryBarrier read[] =
				{
					b_interior.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),

				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eDrawIndirect | vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(read), read }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelTopChild].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

			cmd.dispatchIndirect(b_interior_counter.getInfo().buffer, b_interior_counter.getInfo().offset);
		}

		_label.insert("Make Mid");
		{
			{
				vk::BufferMemoryBarrier read[] =
				{
					b_interior.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, { array_size(read), read }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_MakeVoxel].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
			cmd.pushConstants<int>(m_PL[PipelineLayout_MakeVoxel].get(), vk::ShaderStageFlagBits::eFragment, 0, 2);

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_RP_makevoxel.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_info.reso.x, m_info.reso.z)));
			begin_render_Info.setFramebuffer(m_FB_makevoxel.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindIndexBuffer(model.b_index.getInfo().buffer, model.b_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.bindVertexBuffers(0, model.b_vertex.getInfo().buffer, model.b_vertex.getInfo().offset);
			cmd.drawIndexed(model.m_info.m_primitive_num * 3, 1, 0, 0, 0);

			cmd.endRenderPass();
		}

		_label.insert("Make Mid Child");
		{
			{
				vk::BufferMemoryBarrier barrier[] =
				{
					b_interior.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eDrawIndirect | vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelMidChild].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

			cmd.dispatchIndirect(b_interior_counter.getInfo().buffer, b_interior_counter.getInfo().offset + 16);
		}

		_label.insert("Make Data");
		{
			{
				vk::BufferMemoryBarrier read[] =
				{
					b_interior.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					b_leaf_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, { array_size(read), read }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_MakeVoxel].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
			cmd.pushConstants<int>(m_PL[PipelineLayout_MakeVoxel].get(), vk::ShaderStageFlagBits::eFragment, 0, 3);

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_RP_makevoxel.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_info.reso.x, m_info.reso.z)));
			begin_render_Info.setFramebuffer(m_FB_makevoxel.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindIndexBuffer(model.b_index.getInfo().buffer, model.b_index.getInfo().offset, vk::IndexType::eUint32);
			cmd.bindVertexBuffers(0, model.b_vertex.getInfo().buffer, model.b_vertex.getInfo().offset);
			cmd.drawIndexed(model.m_info.m_primitive_num * 3, 1, 0, 0, 0);

			cmd.endRenderPass();
		}

		_label.insert("Make HashMapMask");
		{
			if(0)
			{
				vk::BufferMemoryBarrier barrier[] =
				{
					b_interior.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eDrawIndirect | vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeHashMapMask].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

			auto num = uvec3(VoxelReso)>>uvec3(6);
//			num = app::calcDipatchGroups(num, uvec3(4));
			cmd.dispatch(num.x, num.y, num.z);
		}

	}
	void execute_RenderVoxel(vk::CommandBuffer cmd, RenderTarget& rt)
	{
		{
			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(rt.m_image);
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader,
				{}, {}, { /*array_size(to_read), to_read*/ }, { image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RenderVoxel].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 1, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 2, { rt.m_descriptor.get() }, {});

		auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(8, 8, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	void executeDebug_RenderVoxel(vk::CommandBuffer cmd, RenderTarget& rt)
	{
		{
			vk::ImageMemoryBarrier image_barrier[1];
			image_barrier[0].setImage(rt.m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Debug_RenderVoxel].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_RenderVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_RenderVoxel].get(), 1, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_RenderVoxel].get(), 2, { rt.m_descriptor.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_RP_debugvoxel.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_info.extent.width, rt.m_info.extent.height)));
		begin_render_Info.setFramebuffer(m_FB_debugvoxel.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		auto num = m_info.reso.xyz();
		cmd.draw(num.x * num.y * num.z, 1, 0, 0);

		cmd.endRenderPass();

	}
};


void dda_f(vec3 P, vec3 Dir)
{
	ivec3 reso = ivec3(1000, 1000, 1);
	vec3 CellSize = vec3(5.f,5.f,1000.f);
	vec3 DirInv;
	DirInv.x = abs(Dir.x) < 0.0001 ? (glm::step(Dir.x, 0.f) * 2. - 1.) * 10000. : (1. / Dir.x);
	DirInv.y = abs(Dir.y) < 0.0001 ? (glm::step(Dir.y, 0.f) * 2. - 1.) * 10000. : (1. / Dir.y);
	DirInv.z = abs(Dir.z) < 0.0001 ? (glm::step(Dir.z, 0.f) * 2. - 1.) * 10000. : (1. / Dir.z);

	float n, f;
	intersection(vec3(0.f), vec3(reso)*CellSize, P, DirInv, n, f);
	vec3 P2 = P + Dir * f;

//	P /= CellSize;
	vec3 deltaT = CellSize * abs(DirInv);
	vec3 t = ((floor(P/ CellSize) + step(vec3(0., 0., 0.), Dir))* CellSize - P) * DirInv;
	ivec3 Cell = ivec3(P/ CellSize);

//	while (all(greaterThanEqual(Cell, ivec3(0))) && all(lessThan(Cell, reso)))
	while (any(notEqual(Cell.xy(), ivec3(P2/CellSize).xy())))
	{
		printf("%4d,%4d,%4d\n", Cell.x, Cell.y, Cell.z);

		int comp = t[0] < t[1] ? 0 : 1;
//		comp = t[comp] < t[2] ? comp : 2;
		vec3 mask = vec3(equal(ivec3(0, 1, 2), ivec3(comp)));

		Cell = sign(Dir)* mask+ vec3(Cell);
		t = deltaT* mask+ t;

	}
//	auto p = closestPointOnLine(vec3(Cell.xy(), P.z) * vec3(CellSize.xy(), 1.f), P.xy(), P2.xy());
	float tt = distance(P.xy(), (vec3(Cell)*CellSize).xy()) / distance(P.xy(), P2.xy());
	auto PP = P + Dir * f*tt;
// 	T LineLength = distance(a, b);
// 	vec<2, T, Q> Vector = point - a;
// 	vec<2, T, Q> LineDirection = (b - a) / LineLength;
// 
// 	// Project Vector to LineDirection to get the distance of point from a
// 	T Distance = dot(Vector, LineDirection);
// 
// 	if (Distance <= T(0)) return a;
// 	if (Distance >= LineLength) return b;
// 	return a + LineDirection * Distance;
	//	t /= deltaT;
	int b = 0;
}

void march(ivec3& Cell, vec3& t, vec3 dir, vec3 deltaT)
{
	float tmin = glm::min(glm::min(t.x, t.y), t.z);
	vec3 mask = vec3(lessThanEqual(t, vec3(tmin)));

	Cell += ivec3(sign(dir) * mask);
	t = deltaT*mask+t;

}

bool MyIntersection(vec3 aabb_min, vec3 aabb_max, vec3 pos, vec3 inv_dir, float& n, float& f, int& dim)
{
	// https://tavianator.com/fast-branchless-raybounding-box-intersections/
	vec3 t1 = (aabb_min - pos) * inv_dir;
	vec3 t2 = (aabb_max - pos) * inv_dir;

	vec3 tmin = min(t1, t2);
	vec3 tmax = max(t1, t2);

	n = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
	f = glm::min(glm::min(tmax.x, tmax.y), tmax.z);
	f = glm::max(f, 0.f);

	int comp = tmax[0] < tmax[1] ? 0 : 1;
	comp = tmax[comp] < tmax[2] ? comp : 2;
	dim = comp;

	return f >= n;
}

void dda_test()
{
	for (int i = 0; i < 5000000; i++)
	{
		if ((i % 1000) == 0)
			printf("%d\n", i);

		float n, f;
		int comp;

		vec3 p_orig = glm::ballRand(250.f) + 250.f;
		vec3 dir = normalize(glm::ballRand(1.f));

		vec3 inv_dir = 1.f / dir;
		vec3 pos = p_orig;
		vec3 deltaT = abs(inv_dir);
		ivec3 Reso = ivec3(500);

/*
		vec3 dir_sign = sign(dir) * 0.5f + 0.5f;
		bvec3 dir_sign_b = greaterThan(dir, vec3(0.f));
		vec3 p_origin = mix(vec3(Reso)-pos, pos, dir_sign);
		pos = p_origin;
		ivec3 Cell = ivec3(floor(pos));
		vec3 t = (1.f - fract(pos)) * abs(inv_dir);

		while (all(greaterThanEqual(Cell, ivec3(0))) && all(lessThan(Cell, Reso)))
		{
			ivec3 top_index = Cell >> ivec3(2);
			vec3 min_ = vec3(top_index << ivec3(2));
			vec3 max_ = vec3((top_index + ivec3(1)) << ivec3(2));

			bool b = MyIntersection(min_, max_, p_origin, abs(inv_dir), n, f, comp);
			if (!b)
			{
				int aaa = 0;
			}
			vec3 comp_mask = vec3(equal(ivec3(0, 1, 2), ivec3(comp)));

			pos = abs(dir) * vec3(f) + p_origin;
			Cell = max(ivec3(floor(pos + comp_mask * 0.5)), Cell);
			t = mix(1.f-fract(pos), vec3(1.), comp_mask) * abs(inv_dir);

			printf("%3d,%3d,%3d\n", Cell.x, Cell.y, Cell.z);

		}
*/

		vec3 dir_sign = sign(dir) * 0.5f + 0.5f;
		bvec3 dir_sign_b = greaterThan(dir, vec3(0.f));
		vec3 p_origin = pos;
		ivec3 Cell = ivec3(floor(pos));
		vec3 t = mix(fract(pos), 1.f-fract(pos), dir_sign) * abs(inv_dir);

		while (all(greaterThanEqual(Cell, ivec3(0))) && all(lessThan(Cell, Reso)))
		{
			ivec3 top_index = Cell >> ivec3(2);
			vec3 min_ = vec3(top_index << ivec3(2));
			vec3 max_ = vec3((top_index + ivec3(1)) << ivec3(2));

			bool b = MyIntersection(min_, max_, p_origin, inv_dir, n, f, comp);
			if (!b)
			{
				int aaa = 0;
			}
			vec3 comp_mask = vec3(equal(ivec3(0, 1, 2), ivec3(comp)));

			pos = dir * vec3(f) + p_origin;
			auto Cell_New = ivec3(floor(pos + dir*comp_mask * 0.5));
			Cell = mix(min(Cell_New, Cell), max(Cell_New, Cell), dir_sign_b);
			t = mix(fract(pos), 1.f - fract(pos), dir_sign);
			t = mix(t, vec3(1.), comp_mask) * abs(inv_dir);

//			printf("%3d,%3d,%3d\n", Cell.x, Cell.y, Cell.z);

		}

	}


}

int main()
{

//	dda_test();
//	voxel_test();

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-1000.f, 500.f, -666.f);
	camera->getData().m_target = vec3(0.f, 200.f, 0.f);
	//	camera->getData().m_position = vec3(1000.f, 220.f, -200.f);
	//	camera->getData().m_target = vec3(1000.f, 220.f, 0.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	auto ctx = std::make_shared<Context>(context);

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

//	Voxel2 voxel(*context, *app.m_window->getFrontBuffer());
//	Voxel1 voxel(*context, *app.m_window->getFrontBuffer());

//	std::shared_ptr<Model> model = Model::LoadModel(*ctx, btr::getResourceAppPath() + "Box.dae");
	std::shared_ptr<Model> model = Model::LoadModel(*ctx, btr::getResourceAppPath() + "Duck.dae");
	Voxel_With_Model voxel_with_model(*ctx, *app.m_window->getFrontBuffer());
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		voxel_with_model.execute_MakeVoxel(cmd, *model);
//		voxel.execute_MakeVoxel(cmd);
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
				cmd_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				{
 					voxel_with_model.execute_MakeVoxel(cmd, *model);
 					voxel_with_model.execute_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
//					voxel_with_model.executeDebug_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
//					voxel.execute_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
//					voxel.executeDebug_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
				}

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

