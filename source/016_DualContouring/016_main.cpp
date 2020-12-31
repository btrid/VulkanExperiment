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
#include <applib/GraphicsResource.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>
#include <applib/sCameraManager.h>
#include <applib/sAppImGui.h>


#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/material.h>

#include <016_DualContouring/texture.h>

struct LDCPoint
{
	float p;
	uint normal;
	int next;
	uint flag;
};
struct DCCell
{
	uvec3 normal;
	uint8_t usepoint;
	u8vec3 xyz;
};

struct DCContext
{
	btr::AllocatedMemory m_ASinstance_memory;

	enum DSL
	{
		DSL_Model,
		DSL_DC,
		DSL_Worker,
		DSL_Num,
	};
	vk::UniqueDescriptorSetLayout m_DSL[DSL_Num];

	btr::AllocatedMemory m_shader_binding_table_memory;
	btr::BufferMemory b_shader_binding_table;
	std::array<vk::StridedDeviceAddressRegionKHR, 4> m_shader_binding_table;

	vk::UniquePipelineLayout m_pipelinelayout_MakeLDC;
	vk::UniquePipeline m_pipeline_MakeLDC;
	vk::UniquePipelineLayout m_pipelinelayout_MakeDC;
	vk::UniquePipeline m_pipeline_MakeDCCell;
	vk::UniquePipeline m_pipeline_MakeDCVertex;
	vk::UniquePipeline m_pipeline_MakeDCFace;

	vk::UniquePipeline m_pipeline_LDC_boolean_add;

	vk::UniquePipeline m_pipeline_makeDCV;

	btr::BufferMemoryEx<DCCell> b_dc_cell;
	btr::BufferMemoryEx<uint32_t> b_dc_cell_hashmap;

	vk::UniqueDescriptorSet m_DS_worker;
// 	struct DCInfo
// 	{
// 		uvec3 m_voxel_reso;
// 	};
	DCContext(btr::Context& ctx)
	{
		m_ASinstance_memory.setup(ctx.m_physical_device, ctx.m_device, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, sizeof(vk::AccelerationStructureInstanceKHR) * 100);

		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eRaygenKHR| vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eAccelerationStructureKHR, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Model] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}
			{
				auto stage = vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_DC] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}

			{
				auto stage = vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Worker] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}

		}

		// raytracing pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DCContext::DSL_Model].get(),
					m_DSL[DCContext::DSL_DC].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipelinelayout_MakeLDC = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DCContext::DSL_DC].get(),
					m_DSL[DCContext::DSL_Worker].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipelinelayout_MakeDC = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

			{
				const uint32_t shaderIndexRaygen = 0;
				const uint32_t shaderIndexMiss = 1;
				const uint32_t shaderIndexClosestHit = 2;

				struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
				{
					{"LDC_Construct.rgen.spv", vk::ShaderStageFlagBits::eRaygenKHR},
					{"LDC_Construct.rmiss.spv", vk::ShaderStageFlagBits::eMissKHR},
					{"LDC_Construct.rchit.spv", vk::ShaderStageFlagBits::eClosestHitKHR},
				};
				std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
				std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
				for (size_t i = 0; i < array_length(shader_param); i++)
				{
					shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
					shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
				}

				//	Setup ray tracing shader groups
				std::array<vk::RayTracingShaderGroupCreateInfoKHR, 3> shader_group;
				shader_group[0].type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
				shader_group[0].generalShader = shaderIndexRaygen;
				shader_group[0].closestHitShader = VK_SHADER_UNUSED_KHR;
				shader_group[0].anyHitShader = VK_SHADER_UNUSED_KHR;
				shader_group[0].intersectionShader = VK_SHADER_UNUSED_KHR;

				shader_group[1].type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
				shader_group[1].generalShader = shaderIndexMiss;
				shader_group[1].closestHitShader = VK_SHADER_UNUSED_KHR;
				shader_group[1].anyHitShader = VK_SHADER_UNUSED_KHR;
				shader_group[1].intersectionShader = VK_SHADER_UNUSED_KHR;

				shader_group[2].type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
				shader_group[2].generalShader = VK_SHADER_UNUSED_KHR;
				shader_group[2].closestHitShader = shaderIndexClosestHit;
				shader_group[2].anyHitShader = VK_SHADER_UNUSED_KHR;
				shader_group[2].intersectionShader = VK_SHADER_UNUSED_KHR;

				vk::RayTracingPipelineCreateInfoKHR rayTracingPipelineCI;
				rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
				rayTracingPipelineCI.pStages = shaderStages.data();
				rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shader_group.size());
				rayTracingPipelineCI.pGroups = shader_group.data();
				rayTracingPipelineCI.maxPipelineRayRecursionDepth = 16;
				rayTracingPipelineCI.layout = m_pipelinelayout_MakeLDC.get();
				m_pipeline_MakeLDC = ctx.m_device.createRayTracingPipelineKHRUnique(vk::DeferredOperationKHR(), vk::PipelineCache(), rayTracingPipelineCI).value;

				// shader binding table
				{
					auto props2 = ctx.m_physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
					auto rayTracingProperties = props2.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

					const uint32_t groupCount = static_cast<uint32_t>(shader_group.size());

					const uint32_t sbtSize = rayTracingProperties.shaderGroupBaseAlignment * groupCount;

					m_shader_binding_table_memory.setup(ctx.m_physical_device, ctx.m_device, vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddressKHR, vk::MemoryPropertyFlagBits::eDeviceLocal, sbtSize);
					b_shader_binding_table = m_shader_binding_table_memory.allocateMemory(sbtSize);

					auto shaderHandleStorage = ctx.m_device.getRayTracingShaderGroupHandlesKHR<uint8_t>(m_pipeline_MakeLDC.get(), 0, groupCount, rayTracingProperties.shaderGroupHandleSize * groupCount);

					std::vector<uint8_t> data(sbtSize);
					for (uint32_t i = 0; i < groupCount; i++)
					{
						memcpy(data.data() + i * rayTracingProperties.shaderGroupBaseAlignment, shaderHandleStorage.data() + i * rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupHandleSize);
					}
					cmd.updateBuffer<uint8_t>(b_shader_binding_table.getInfo().buffer, b_shader_binding_table.getInfo().offset, data);

					vk::BufferMemoryBarrier barrier[] =
					{
						b_shader_binding_table.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});


//					m_shader_binding_table[0].buffer = b_shader_binding_table.getInfo().buffer;
					auto device_address = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfoKHR().setBuffer(b_shader_binding_table.getInfo().buffer)) + b_shader_binding_table.getInfo().offset;
					m_shader_binding_table[0].deviceAddress = device_address;
					m_shader_binding_table[0].stride = rayTracingProperties.shaderGroupBaseAlignment;
					m_shader_binding_table[0].size = rayTracingProperties.shaderGroupBaseAlignment;

					m_shader_binding_table[1].deviceAddress = device_address + rayTracingProperties.shaderGroupBaseAlignment * 1;
					m_shader_binding_table[1].stride = rayTracingProperties.shaderGroupBaseAlignment;
					m_shader_binding_table[1].size = rayTracingProperties.shaderGroupBaseAlignment;

					m_shader_binding_table[2].deviceAddress = device_address + + rayTracingProperties.shaderGroupBaseAlignment * 2;
					m_shader_binding_table[2].stride = rayTracingProperties.shaderGroupBaseAlignment;
					m_shader_binding_table[2].size = rayTracingProperties.shaderGroupBaseAlignment;

				}

			}

			// compute pipeline
			{
				struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
				{
					{"DC_MakeDCCell.comp.spv", vk::ShaderStageFlagBits::eCompute},
					{"DC_MakeDCVertex.comp.spv", vk::ShaderStageFlagBits::eCompute},
					{"DC_MakeDCVFace.comp.spv", vk::ShaderStageFlagBits::eCompute},
					{"LDC_BooleanAdd2.comp.spv", vk::ShaderStageFlagBits::eCompute},
				};
				std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
				std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
				for (size_t i = 0; i < array_length(shader_param); i++)
				{
					shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
					shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
				}

				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(shaderStages[0])
					.setLayout(m_pipelinelayout_MakeDC.get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shaderStages[1])
					.setLayout(m_pipelinelayout_MakeDC.get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shaderStages[2])
					.setLayout(m_pipelinelayout_MakeDC.get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shaderStages[3])
					.setLayout(m_pipelinelayout_MakeLDC.get()),
				};
				m_pipeline_MakeDCCell = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
				m_pipeline_MakeDCVertex = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
				m_pipeline_MakeDCFace = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
				m_pipeline_LDC_boolean_add = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;

			}

			// descriptor set
			{
				b_dc_cell = ctx.m_storage_memory.allocateMemory<DCCell>(256 * 256 * 256);
				b_dc_cell_hashmap = ctx.m_storage_memory.allocateMemory<uint32_t>(256 * 256 * 256 / 32);

				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DCContext::DSL_Worker].get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_DS_worker = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);


				vk::DescriptorBufferInfo storages[] =
				{
					b_dc_cell.getInfo(),
					b_dc_cell_hashmap.getInfo(),
				};

				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_DS_worker.get()),
				};
				ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}

	}
};

struct ModelParam
{
	float scale;
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
	vk::UniqueAccelerationStructureKHR m_BLAS;
	vk::UniqueAccelerationStructureKHR m_TLAS;

	vk::UniqueDescriptorSet m_DS_Model;

	Info m_info;

	static std::shared_ptr<Model> LoadModel(btr::Context& ctx, DCContext& dc_ctx, const std::string& filename, const ModelParam& param)
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


		auto vertex = ctx.m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
		auto normal = ctx.m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
		auto index = ctx.m_staging_memory.allocateMemory(numIndex * sizeof(uint32_t), true);

		uint32_t index_offset = 0;
		uint32_t vertex_offset = 0;
		Info info{ .m_aabb_min = vec4{999.f}, .m_aabb_max = vec4{-999.f}, .m_voxel_reso=uvec3(256) };

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
				mesh->mVertices[v] *= param.scale;
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
			info.m_aabb_min -= 0.01f;
			info.m_aabb_max += 0.01f;
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
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR| vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, {}, { array_size(barrier), barrier }, {});


		// build BLAS
		{
			std::array<vk::AccelerationStructureGeometryKHR, 1> accelerationStructureGeometry;
			accelerationStructureGeometry[0].flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
			accelerationStructureGeometry[0].geometryType = vk::GeometryTypeKHR::eTriangles;
			accelerationStructureGeometry[0].geometry.triangles.maxVertex = model->m_info.m_vertex_num;
			accelerationStructureGeometry[0].geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
			accelerationStructureGeometry[0].geometry.triangles.vertexData.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfoKHR().setBuffer(model->b_vertex.getInfo().buffer)) + model->b_vertex.getInfo().offset;

			accelerationStructureGeometry[0].geometry.triangles.vertexStride = sizeof(aiVector3D);
			accelerationStructureGeometry[0].geometry.triangles.indexType = vk::IndexType::eUint32;
			accelerationStructureGeometry[0].geometry.triangles.indexData.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfoKHR().setBuffer(model->b_index.getInfo().buffer)) + model->b_index.getInfo().offset;

			vk::AccelerationStructureBuildGeometryInfoKHR AS_buildinfo;
			AS_buildinfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
			AS_buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild;
			AS_buildinfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
			AS_buildinfo.geometryCount = 1;
			AS_buildinfo.pGeometries = accelerationStructureGeometry.data();

			std::vector<uint32_t> primitives = { model->m_info.m_primitive_num };
			auto size_info = ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, AS_buildinfo, primitives);


			auto AS_buffer = ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
			vk::AccelerationStructureCreateInfoKHR accelerationCI;
			accelerationCI.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
			accelerationCI.buffer = AS_buffer.getInfo().buffer;
			accelerationCI.offset = AS_buffer.getInfo().offset;
			accelerationCI.size =AS_buffer.getInfo().range;
			model->m_BLAS = ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);

			auto scratch_buffer = ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize, true);

			AS_buildinfo.dstAccelerationStructure = model->m_BLAS.get();
			AS_buildinfo.scratchData.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo(scratch_buffer.getInfo().buffer)) + scratch_buffer.getInfo().offset;

			vk::AccelerationStructureBuildRangeInfoKHR acceleration_buildrangeinfo;
			acceleration_buildrangeinfo.primitiveCount = model->m_info.m_primitive_num;
			acceleration_buildrangeinfo.primitiveOffset = 0;
			acceleration_buildrangeinfo.firstVertex = 0;
			acceleration_buildrangeinfo.transformOffset = 0;

			std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> ranges = { &acceleration_buildrangeinfo };
			cmd.buildAccelerationStructuresKHR({ AS_buildinfo }, ranges);
		}
		// build TLAS
		{

			vk::TransformMatrixKHR transform_matrix =
			{
				std::array<std::array<float, 4>, 3>
				{
					std::array<float, 4>{1.0f, 0.0f, 0.0f, 0.0f},
					std::array<float, 4>{0.0f, 1.0f, 0.0f, 0.0f},
					std::array<float, 4>{0.0f, 0.0f, 1.0f, 0.0f},
				}
			};

			vk::AccelerationStructureInstanceKHR instance;
			instance.transform = transform_matrix;
			instance.instanceCustomIndex = 0;
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = ctx.m_device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(model->m_BLAS.get()));

			auto instance_buffer = dc_ctx.m_ASinstance_memory.allocateMemory(sizeof(instance));

			cmd.updateBuffer<vk::AccelerationStructureInstanceKHR>(instance_buffer.getInfo().buffer, instance_buffer.getInfo().offset, { instance });
			vk::BufferMemoryBarrier barrier[] = {
				instance_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});


			auto device_address = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo().setBuffer(instance_buffer.getInfo().buffer))+ instance_buffer.getInfo().offset;

			vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
			accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
			accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
			vk::AccelerationStructureGeometryInstancesDataKHR instance_data;
			instance_data.data.deviceAddress = device_address;
			accelerationStructureGeometry.geometry.instances = instance_data;

			std::vector<vk::AccelerationStructureGeometryKHR> acceleration_geometries = { accelerationStructureGeometry };


			vk::AccelerationStructureBuildGeometryInfoKHR AS_buildinfo;
			AS_buildinfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
			AS_buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild;
			AS_buildinfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
	//			accelerationBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
//			AS_buildinfo.setGeometries({ accelerationStructureGeometry });
			AS_buildinfo.geometryCount = 1;
			AS_buildinfo.pGeometries = acceleration_geometries.data();

			std::vector<uint32_t> primitives = { model->m_info.m_primitive_num };
			auto size_info = ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, AS_buildinfo, primitives);
			auto scratch_buffer = ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize, true);

			auto AS_buffer = ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
			vk::AccelerationStructureCreateInfoKHR accelerationCI;
			accelerationCI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
			accelerationCI.buffer = AS_buffer.getInfo().buffer;
			accelerationCI.offset = AS_buffer.getInfo().offset;
			accelerationCI.size = AS_buffer.getInfo().range;
			model->m_TLAS = ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);

			AS_buildinfo.dstAccelerationStructure = model->m_TLAS.get();
			AS_buildinfo.scratchData.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo(scratch_buffer.getInfo().buffer)) + scratch_buffer.getInfo().offset;

			vk::AccelerationStructureBuildRangeInfoKHR accelerationBuildOffsetInfo;
			accelerationBuildOffsetInfo.primitiveCount = 1;
			accelerationBuildOffsetInfo.primitiveOffset = 0;
			accelerationBuildOffsetInfo.firstVertex = 0;
			accelerationBuildOffsetInfo.transformOffset = 0;

			std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> offset = { &accelerationBuildOffsetInfo };
			cmd.buildAccelerationStructuresKHR({ AS_buildinfo }, offset);
		}

		{
			vk::DescriptorSetLayout layouts[] =
			{
				dc_ctx.m_DSL[DCContext::DSL_Model].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			model->m_DS_Model = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

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
			vk::WriteDescriptorSetAccelerationStructureKHR acceleration;
			acceleration.accelerationStructureCount = 1;
			acceleration.pAccelerationStructures = &model->m_TLAS.get();

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
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eAccelerationStructureKHR)
				.setDescriptorCount(1)
				.setPNext(&acceleration)
				.setDstBinding(5)
				.setDstSet(model->m_DS_Model.get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}

		return model;
	}

};


struct DCModel
{
	// https://www.4gamer.net/games/269/G026934/20170406111/
	// LDC = Layered Depth Cube
	// DCV = Dual Contouring Vertex
	vk::UniqueDescriptorSet m_DS_DC;
	btr::BufferMemoryEx<int32_t> b_ldc_counter;
	btr::BufferMemoryEx<int32_t> b_ldc_point_link_head;
	btr::BufferMemoryEx<LDCPoint> b_ldc_point;
	btr::BufferMemoryEx<u8vec4> b_dc_vertex;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> b_dc_index_counter;
	btr::BufferMemoryEx<u8vec4> b_dc_index;

	static std::shared_ptr<DCModel> Construct(btr::Context& ctx, DCContext& dc_ctx)
	{
		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

		auto dc_model = std::make_shared<DCModel>();
		{
			vk::DescriptorSetLayout layouts[] = 
			{
				dc_ctx.m_DSL[DCContext::DSL_DC].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			dc_model->m_DS_DC = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			dc_model->b_ldc_counter = ctx.m_storage_memory.allocateMemory<int>(1);
			dc_model->b_ldc_point_link_head = ctx.m_storage_memory.allocateMemory<int>(256 * 256 *3);
			dc_model->b_ldc_point = ctx.m_storage_memory.allocateMemory<LDCPoint>(256 * 256 *3*4);

			dc_model->b_dc_vertex = ctx.m_storage_memory.allocateMemory<u8vec4>(256 * 256 * 256);
			dc_model->b_dc_index_counter = ctx.m_storage_memory.allocateMemory<vk::DrawIndirectCommand>(1);
			dc_model->b_dc_index = ctx.m_storage_memory.allocateMemory<u8vec4>(256 * 256 * 256);

			vk::DescriptorBufferInfo storages[] =
			{
				dc_model->b_ldc_counter.getInfo(),
				dc_model->b_ldc_point_link_head.getInfo(),
				dc_model->b_ldc_point.getInfo(),
				dc_model->b_dc_vertex.getInfo(),
				dc_model->b_dc_index_counter.getInfo(),
				dc_model->b_dc_index.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(dc_model->m_DS_DC.get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		{
			cmd.fillBuffer(dc_model->b_ldc_counter.getInfo().buffer, dc_model->b_ldc_counter.getInfo().offset, dc_model->b_ldc_counter.getInfo().range, 0);
			vk::BufferMemoryBarrier barrier[] =
			{
				dc_model->b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, {}, { array_size(barrier), barrier }, {});
		}

		return dc_model;
	}

	static void CreateLDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model, Model& model);

	static void CreateDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model);

};

void DCModel::CreateLDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model, Model& model)
{

// 	cmd.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, dc_ctx.m_pipeline_MakeLDC.get());
// 	cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, dc_ctx.m_pipelinelayout_MakeLDC.get(), 0, { model.m_DS_Model.get() }, {});
// 	cmd.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, dc_ctx.m_pipelinelayout_MakeLDC.get(), 1, { dc_model.m_DS_DC.get() }, {});
// 
// 	cmd.traceRaysKHR(
// 		dc_ctx.m_shader_binding_table[0],
// 		dc_ctx.m_shader_binding_table[1],
// 		dc_ctx.m_shader_binding_table[2],
// 		dc_ctx.m_shader_binding_table[3],
// 		model.m_info.m_voxel_reso.x,
// 		model.m_info.m_voxel_reso.y,
// 		3);

// 	{
// 		cmd.fillBuffer(dc_model.b_ldc_counter.getInfo().buffer, dc_model.b_ldc_counter.getInfo().offset, dc_model.b_ldc_counter.getInfo().range, 0);
// 		vk::BufferMemoryBarrier barrier[] =
// 		{
// 			dc_model.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
// 		};
// 		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
// 	}

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_LDC_boolean_add.get());
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeLDC.get(), 0, { model.m_DS_Model.get() }, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeLDC.get(), 1, { dc_model.m_DS_DC.get() }, {});

	cmd.dispatch(4, 256, 3);

}

void DCModel::CreateDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model)
{
	{
		vk::BufferMemoryBarrier barrier[] =
		{
			dc_model.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

	}

	// Make DC Cell
	{

		cmd.fillBuffer(dc_ctx.b_dc_cell.getInfo().buffer, dc_ctx.b_dc_cell.getInfo().offset, dc_ctx.b_dc_cell.getInfo().range, 0);
		cmd.fillBuffer(dc_ctx.b_dc_cell_hashmap.getInfo().buffer, dc_ctx.b_dc_cell_hashmap.getInfo().offset, dc_ctx.b_dc_cell_hashmap.getInfo().range, -1);
		cmd.updateBuffer<vk::DrawIndirectCommand>(dc_model.b_dc_index_counter.getInfo().buffer, dc_model.b_dc_index_counter.getInfo().offset, { vk::DrawIndirectCommand(0,1,0,0) });

		vk::BufferMemoryBarrier barrier[] =
		{
			dc_ctx.b_dc_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_MakeDCCell.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 0, { dc_model.m_DS_DC.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 1, { dc_ctx.m_DS_worker.get() }, {});

		cmd.dispatch(4, 256, 3);
	}


	// Make DC Vertex
	{

		vk::BufferMemoryBarrier barrier[] =
		{
			dc_ctx.b_dc_cell.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			dc_ctx.b_dc_cell_hashmap.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_MakeDCVertex.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 0, { dc_model.m_DS_DC.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 1, { dc_ctx.m_DS_worker.get() }, {});

		cmd.dispatch(4, 256, 256);
	}

	// Make DC Face
	{

		vk::BufferMemoryBarrier barrier[] =
		{
			dc_model.b_dc_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_dc_index_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_MakeDCFace.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 0, { dc_model.m_DS_DC.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 1, { dc_ctx.m_DS_worker.get() }, {});

		cmd.dispatch(4, 256, 256);

	}

	{
		vk::BufferMemoryBarrier barrier[] =
		{
			dc_model.b_dc_index_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { array_size(barrier), barrier }, {});

	}
}

struct DCFunctionLibrary
{
	enum
	{
		Pipeline_Boolean_Add,
		Pipeline_Boolean_Sub,
		Pipeline_Num,
	};
	vk::UniquePipelineLayout m_PL_boolean;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	DCFunctionLibrary(btr::Context& ctx, DCContext& dc_ctx)
	{
		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] =
			{
				dc_ctx.m_DSL[DCContext::DSL_DC].get(),
				dc_ctx.m_DSL[DCContext::DSL_Model].get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_PL_boolean = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		// compute pipeline
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"LDC_BooleanAdd.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"LDC_BooleanSub.comp.spv", vk::ShaderStageFlagBits::eCompute},
			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[0])
				.setLayout(m_PL_boolean.get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[1])
				.setLayout(m_PL_boolean.get()),
			};
			m_pipeline[Pipeline_Boolean_Add] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_Boolean_Sub] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
		}
	}

	void executeBooleanAdd(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& base, DCModel& boolean)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Boolean_Add].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 0, { base.m_DS_DC.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 1, { boolean.m_DS_DC.get() }, {});

		cmd.dispatch(4, 256, 3);

		vk::BufferMemoryBarrier barrier[] =
		{
			base.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			base.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			base.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		DCModel::CreateDCModel(cmd, dc_ctx, base);

	}
	void executeBooleanSub(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& base, DCModel& boolean)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Boolean_Sub].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 0, { base.m_DS_DC.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 1, { boolean.m_DS_DC.get() }, {});

		cmd.dispatch(4, 256, 3);

		vk::BufferMemoryBarrier barrier[] =
		{
			base.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			base.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			base.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		DCModel::CreateDCModel(cmd, dc_ctx, base);
	}
};
struct Material
{
	TextureResource albedo_texture;
};
struct Renderer
{
	vk::UniqueRenderPass m_TestRender_pass;
	vk::UniqueFramebuffer m_TestRender_framebuffer;
	vk::UniquePipeline m_pipeline_TestRender;
	vk::UniquePipeline m_pipeline_Render;
	vk::UniquePipeline m_pipeline_RenderModel;
	vk::UniquePipelineLayout m_pl;

	std::array<Material, 100> m_world_material;
	vk::UniqueDescriptorSetLayout m_DSL_Rendering;
	std::array<vk::UniqueDescriptorSet, sGlobal::BUFFER_COUNT_MAX> m_DS_Rendering;

	Renderer(btr::Context& ctx, DCContext& dc_ctx, RenderTarget& rt)
	{
		// descriptor set layout
		{
			vk::DescriptorSetLayoutBinding accelerationStructureLayoutBinding;
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 100, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL_Rendering = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);

		}

		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL_Rendering.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			for (auto& ds : m_DS_Rendering)
			{
				ds = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			}

		}

		{

			auto tex = TextureResource::read_png_file(ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\renga_pattern.png");
			m_world_material[0].albedo_texture = std::move(tex);

			auto def = vk::DescriptorImageInfo(sGraphicsResource::Order().getSampler(sGraphicsResource::BASIC_SAMPLER_LINER), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal);
			std::array<vk::DescriptorImageInfo, 100> images;
			images.fill(def);

			images[0].setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(m_world_material[0].albedo_texture.m_image_view.get()).setSampler(m_world_material[0].albedo_texture.m_sampler.get());

			std::array<vk::WriteDescriptorSet, 100> write;
			for (int i=0;i<100;i++)
			{
				write[i] = vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(1)
					.setPImageInfo(images.data() + i)
					.setDstBinding(0)
					.setDstArrayElement(i)
					.setDstSet(m_DS_Rendering[0].get());
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write.data(), 0, nullptr);
		}		

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] =
			{
				dc_ctx.m_DSL[DCContext::DSL_DC].get(),
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				m_DSL_Rendering.get(),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pl = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

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

				m_TestRender_pass = ctx.m_device.createRenderPassUnique(renderpass_info);

				{
					vk::ImageView view[] = {
						rt.m_view,
						rt.m_depth_view,
					};
					vk::FramebufferCreateInfo framebuffer_info;
					framebuffer_info.setRenderPass(m_TestRender_pass.get());
					framebuffer_info.setAttachmentCount(array_length(view));
					framebuffer_info.setPAttachments(view);
					framebuffer_info.setWidth(rt.m_info.extent.width);
					framebuffer_info.setHeight(rt.m_info.extent.height);
					framebuffer_info.setLayers(1);

					m_TestRender_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);
				}
			}

			// graphics pipeline
			if (0)
			{
				struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
				{
					{"DCDebug_Rendering.vert.spv", vk::ShaderStageFlagBits::eVertex},
					{"DCDebug_Rendering.frag.spv", vk::ShaderStageFlagBits::eFragment},
				};
				std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
				std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
				for (size_t i = 0; i < array_length(shader_param); i++)
				{
					shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
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
				blend_state.setBlendEnable(VK_TRUE);
				blend_state.setColorBlendOp(vk::BlendOp::eAdd);
				blend_state.setSrcColorBlendFactor(vk::BlendFactor::eOne);
				blend_state.setDstColorBlendFactor(vk::BlendFactor::eOne);
				blend_state.setAlphaBlendOp(vk::BlendOp::eAdd);
				blend_state.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
				blend_state.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
				blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB);

				vk::PipelineColorBlendStateCreateInfo blend_info;
				blend_info.setAttachmentCount(1);
				blend_info.setPAttachments(&blend_state);

				vk::PipelineVertexInputStateCreateInfo vertex_input_info;

				vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
					vk::GraphicsPipelineCreateInfo()
					.setStageCount(array_length(shaderStages))
					.setPStages(shaderStages.data())
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&assembly_info)
					.setPViewportState(&viewportInfo)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pl.get())
					.setRenderPass(m_TestRender_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info);
				m_pipeline_TestRender = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;
			}

			{
				struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
				{
					{"DC_Rendering.vert.spv", vk::ShaderStageFlagBits::eVertex},
					{"DC_Rendering.geom.spv", vk::ShaderStageFlagBits::eGeometry},
					{"DC_Rendering.frag.spv", vk::ShaderStageFlagBits::eFragment},
					{"DC_RenderModel.vert.spv", vk::ShaderStageFlagBits::eVertex},
					{"DC_RenderModel.frag.spv", vk::ShaderStageFlagBits::eFragment},

				};
				std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
				std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
				for (size_t i = 0; i < array_length(shader_param); i++)
				{
					shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
					shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
				}

				// assembly
				vk::PipelineInputAssemblyStateCreateInfo assembly_info;
				assembly_info.setPrimitiveRestartEnable(VK_FALSE);
				assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

				vk::PipelineInputAssemblyStateCreateInfo model_assembly_info;
				model_assembly_info.setPrimitiveRestartEnable(VK_FALSE);
				model_assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

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
					.setStageCount(3)
					.setPStages(shaderStages.data())
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&assembly_info)
					.setPViewportState(&viewportInfo)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pl.get())
					.setRenderPass(m_TestRender_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info);
				m_pipeline_Render = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

				graphics_pipeline_info =
					vk::GraphicsPipelineCreateInfo()
					.setStageCount(2)
					.setPStages(shaderStages.data()+3)
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&model_assembly_info)
					.setPViewportState(&viewportInfo)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pl.get())
					.setRenderPass(m_TestRender_pass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info);
				m_pipeline_RenderModel = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

			}
		}
	}
	void ExecuteTestRender(vk::CommandBuffer cmd, DCContext& dc_ctx, DCModel& ldc_model, RenderTarget& rt)
	{
		{
			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(rt.m_image);
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { image_barrier });
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_TestRender_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_info.extent.width, rt.m_info.extent.height)));
		begin_render_Info.setFramebuffer(m_TestRender_framebuffer.get());
		begin_render_Info.setClearValueCount(1);
		auto color = vk::ClearValue(vk::ClearColorValue(std::array<uint32_t, 4>{}));
		begin_render_Info.setPClearValues(&color);
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl.get(), 0, ldc_model.m_DS_DC.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl.get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline_TestRender.get());
		cmd.draw(64 * 64 * 64, 1, 0, 0);

		cmd.endRenderPass();

	}
	void ExecuteRenderLDCModel(vk::CommandBuffer cmd, DCContext& dc_ctx, DCModel& ldc_model, RenderTarget& rt)
	{
		{
			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(rt.m_image);
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { image_barrier });
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_TestRender_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_info.extent.width, rt.m_info.extent.height)));
		begin_render_Info.setFramebuffer(m_TestRender_framebuffer.get());
		begin_render_Info.setClearValueCount(1);
		auto color = vk::ClearValue(vk::ClearColorValue(std::array<uint32_t, 4>{55, 55, 55, 0}));
		begin_render_Info.setPClearValues(&color);
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl.get(), 0, ldc_model.m_DS_DC.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl.get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl.get(), 2, m_DS_Rendering[0].get(), {});

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline_Render.get());
		cmd.drawIndirect(ldc_model.b_dc_index_counter.getInfo().buffer, ldc_model.b_dc_index_counter.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

		cmd.endRenderPass();

	}
	void ExecuteRenderModel(vk::CommandBuffer cmd, DCContext& dc_ctx, DCModel& ldc_model, Model& model, RenderTarget& rt)
	{
		{
			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(rt.m_image);
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { image_barrier });
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_TestRender_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_info.extent.width, rt.m_info.extent.height)));
		begin_render_Info.setFramebuffer(m_TestRender_framebuffer.get());
		begin_render_Info.setClearValueCount(1);
		auto color = vk::ClearValue(vk::ClearColorValue(std::array<uint32_t, 4>{55, 55, 55, 0}));
		begin_render_Info.setPClearValues(&color);
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl.get(), 0, ldc_model.m_DS_DC.get(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pl.get(), 1, sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA), {});

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline_RenderModel.get());
		cmd.drawIndirect(model.b_draw_cmd.getInfo().buffer, model.b_draw_cmd.getInfo().offset, 1, sizeof(vk::DrawIndirectCommand));

		cmd.endRenderPass();

	}
};

#include <016_DualContouring/test.h>
int main()
{
//	test();

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(50.f, 50.f, -200.f);
	camera->getData().m_target = glm::vec3(50.f, 50.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	auto dc_ctx = std::make_shared<DCContext>(*context);
//	DCFunctionLibrary dc_fl(*context, *dc_ctx);

	auto model_box = Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Box.dae", {50.f});
	auto model = Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Duck.dae", {1.f});

	auto dc_model = DCModel::Construct(*context, *dc_ctx);
	Renderer renderer(*context, *dc_ctx, *app.m_window->getFrontBuffer());

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		DCModel::CreateLDCModel(cmd, *dc_ctx, *dc_model, *model);
		DCModel::CreateDCModel(cmd, *dc_ctx, *dc_model);
//		dc_fl.executeBooleanSub(cmd, *dc_ctx, *dc_model, *dc_model_box);
//		dc_fl.executeBooleanAdd(cmd, *dc_ctx, *dc_model, *model);
//		dc_fl.executeBooleanSub(cmd, *dc_ctx, *dc_model, *model_box);
	}

	app.setup();

	while (true)
	{
		cStopWatch time;
		dc_ctx->m_ASinstance_memory.gc();
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
//				cCamera::sCamera::Order().getCameraList()[0]->control(app.m_window->getInput(), 0.016f);

				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
//				renderer.ExecuteTestRender(cmd, *ldc_ctx, *ldc_model, *app.m_window->getFrontBuffer());
				renderer.ExecuteRenderLDCModel(cmd, *dc_ctx, *dc_model, *app.m_window->getFrontBuffer());
//				renderer.ExecuteRenderLDCModel(cmd, *dc_ctx, *dc_model_box, *app.m_window->getFrontBuffer());
				//				renderer.ExecuteRenderModel(cmd, *ldc_ctx, *ldc_model, *model, *app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.3fms\n", time.getElapsedTimeAsMilliSeconds());
	}
}