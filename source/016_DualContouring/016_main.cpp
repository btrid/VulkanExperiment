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
		m_ASinstance_memory.setup(ctx.m_physical_device, ctx.m_device, vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, sizeof(vk::AccelerationStructureInstanceKHR) * 1000);

		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eCompute;
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
				auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
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

			// compute pipeline
			{
				struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
				{
					{"DC_MakeDCCell.comp.spv", vk::ShaderStageFlagBits::eCompute},
					{"DC_MakeDCVertex.comp.spv", vk::ShaderStageFlagBits::eCompute},
					{"DC_MakeDCVFace.comp.spv", vk::ShaderStageFlagBits::eCompute},
					{"LDC_Construct.comp.spv", vk::ShaderStageFlagBits::eCompute},
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

#define Voxel_Reso uvec3(256)
#define LDC_Reso uvec3(Voxel_Reso.xx(), 3)
#define Voxel_Block_Size vec3(512.f)

struct ModelParam
{
	float scale;
};

struct ModelInstance
{
	vec4 pos;
	vec4 dir;
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
				mesh->mVertices[v] += aiVector3D(0.5f);
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
//  			info.m_aabb_min -= 0.01f;
//  			info.m_aabb_max += 0.01f;
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
					std::array<float, 4>{1.f, 0.f, 0.f, -(model->m_info.m_aabb_min.x-0.01f)},
					std::array<float, 4>{0.f, 1.f, 0.f, -(model->m_info.m_aabb_min.y-0.01f)},
					std::array<float, 4>{0.f, 0.f, 1.f, -(model->m_info.m_aabb_min.z-0.01f)},
				}
			};

			vk::AccelerationStructureInstanceKHR instance;
			instance.transform = transform_matrix;
			instance.instanceCustomIndex = 0;
			instance.mask = 0xFF;
			instance.instanceShaderBindingTableRecordOffset = 0;
			instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
			instance.accelerationStructureReference = ctx.m_device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(model->m_BLAS.get()));

			auto instance_buffer = dc_ctx.m_ASinstance_memory.allocateMemory<vk::AccelerationStructureInstanceKHR>(1, true);

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
			AS_buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
			AS_buildinfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
			AS_buildinfo.srcAccelerationStructure = vk::AccelerationStructureKHR();
//			AS_buildinfo.setGeometries({ accelerationStructureGeometry });
			AS_buildinfo.geometryCount = 1;
			AS_buildinfo.pGeometries = acceleration_geometries.data();

			std::vector<uint32_t> primitives = { model->m_info.m_primitive_num };
			auto size_info = ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, AS_buildinfo, primitives);

			auto AS_buffer = ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
			vk::AccelerationStructureCreateInfoKHR accelerationCI;
			accelerationCI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
			accelerationCI.buffer = AS_buffer.getInfo().buffer;
			accelerationCI.offset = AS_buffer.getInfo().offset;
			accelerationCI.size = AS_buffer.getInfo().range;
			model->m_TLAS = ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);
			AS_buildinfo.dstAccelerationStructure = model->m_TLAS.get();

			auto scratch_buffer = ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize, true);
			AS_buildinfo.scratchData.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo(scratch_buffer.getInfo().buffer)) + scratch_buffer.getInfo().offset;

			vk::AccelerationStructureBuildRangeInfoKHR range;
			range.primitiveCount = 1;
			range.primitiveOffset = 0;
			range.firstVertex = 0;
			range.transformOffset = 0;

			cmd.buildAccelerationStructuresKHR({ AS_buildinfo }, { &range });
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

	static void UpdateTLAS(vk::CommandBuffer& cmd, btr::Context& ctx, DCContext& dc_ctx, Model& model, const vk::TransformMatrixKHR& transform_matrix)
	{

// 		vk::TransformMatrixKHR transform_matrix =
// 		{
// 			std::array<std::array<float, 4>, 3>
// 			{
// 				std::array<float, 4>{1.f, 0.f, 0.f, 0.0f},
// 				std::array<float, 4>{0.f, 1.f, 0.f, 0.0f},
// 				std::array<float, 4>{0.f, 0.f, 1.f, 0.0f},
// 			}
// 		};

		vk::AccelerationStructureInstanceKHR instance;
		instance.transform = transform_matrix;
		instance.instanceCustomIndex = 0;
		instance.mask = 0xFF;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = ctx.m_device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(model.m_BLAS.get()));

		auto instance_buffer = dc_ctx.m_ASinstance_memory.allocateMemory<vk::AccelerationStructureInstanceKHR>(1, true);

		cmd.updateBuffer<vk::AccelerationStructureInstanceKHR>(instance_buffer.getInfo().buffer, instance_buffer.getInfo().offset, { instance });
		vk::BufferMemoryBarrier barrier[] = {
			instance_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});

		auto device_address = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo().setBuffer(instance_buffer.getInfo().buffer)) + instance_buffer.getInfo().offset;

		vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
		accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
		accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
		vk::AccelerationStructureGeometryInstancesDataKHR instance_data;
		instance_data.data.deviceAddress = device_address;
		accelerationStructureGeometry.geometry.instances = instance_data;

		std::vector<vk::AccelerationStructureGeometryKHR> acceleration_geometries = { accelerationStructureGeometry };

		vk::AccelerationStructureBuildGeometryInfoKHR AS_buildinfo;
		AS_buildinfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		AS_buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
		AS_buildinfo.mode = vk::BuildAccelerationStructureModeKHR::eUpdate;
		AS_buildinfo.srcAccelerationStructure = model.m_TLAS.get();
		AS_buildinfo.dstAccelerationStructure = model.m_TLAS.get();
		AS_buildinfo.geometryCount = 1;
		AS_buildinfo.pGeometries = acceleration_geometries.data();

		std::vector<uint32_t> primitives = { model.m_info.m_primitive_num };
		auto size_info = ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, AS_buildinfo, primitives);

		auto AS_buffer = ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
		vk::AccelerationStructureCreateInfoKHR accelerationCI;
		accelerationCI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		accelerationCI.buffer = AS_buffer.getInfo().buffer;
		accelerationCI.offset = AS_buffer.getInfo().offset;
		accelerationCI.size = AS_buffer.getInfo().range;

		auto scratch_buffer = ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize, true);
		AS_buildinfo.scratchData.deviceAddress = ctx.m_device.getBufferAddress(vk::BufferDeviceAddressInfo(scratch_buffer.getInfo().buffer)) + scratch_buffer.getInfo().offset;

		vk::AccelerationStructureBuildRangeInfoKHR range;
		range.primitiveCount = 1;
		range.primitiveOffset = 0;
		range.firstVertex = 0;
		range.transformOffset = 0;

		cmd.buildAccelerationStructuresKHR({ AS_buildinfo }, { &range });

	}
};


struct DCModel
{
	struct Info
	{
		uvec4 m_voxel_reso;
		vec4 m_voxel_size;
		int32_t m_ldc_point_num;
	};
	// https://www.4gamer.net/games/269/G026934/20170406111/
	// LDC = Layered Depth Cube
	// DCV = Dual Contouring Vertex
	vk::UniqueDescriptorSet m_DS_DC;

	btr::BufferMemoryEx<Info> u_DCModel_info;
	btr::BufferMemoryEx<ivec2> b_ldc_counter;
	btr::BufferMemoryEx<int32_t> b_ldc_point_link_head;
	btr::BufferMemoryEx<int32_t> b_ldc_point_link_next;
	btr::BufferMemoryEx<LDCPoint> b_ldc_point;
	btr::BufferMemoryEx<int32_t> b_ldc_point_free;
	btr::BufferMemoryEx<u8vec4> b_dc_vertex;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> b_dc_index_counter;
	btr::BufferMemoryEx<u8vec4> b_dc_index;

	Info m_DCModel_info;
	static std::shared_ptr<DCModel> Construct(btr::Context& ctx, DCContext& dc_ctx)
	{
		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

		auto dc_model = std::make_shared<DCModel>();
		{
			dc_model->m_DCModel_info.m_ldc_point_num = 256 * 256 * 3 * 4;
			dc_model->m_DCModel_info.m_voxel_reso = uvec4(256);
			dc_model->m_DCModel_info.m_voxel_size = vec4(512.f);
			vk::DescriptorSetLayout layouts[] =
			{
				dc_ctx.m_DSL[DCContext::DSL_DC].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			dc_model->m_DS_DC = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			dc_model->u_DCModel_info = ctx.m_uniform_memory.allocateMemory<Info>(1);
			dc_model->b_ldc_counter = ctx.m_storage_memory.allocateMemory<ivec2>(1);
			dc_model->b_ldc_point_link_head = ctx.m_storage_memory.allocateMemory<int>(256 * 256 * 3);
			dc_model->b_ldc_point_link_next = ctx.m_storage_memory.allocateMemory<int>(dc_model->m_DCModel_info.m_ldc_point_num);
			dc_model->b_ldc_point = ctx.m_storage_memory.allocateMemory<LDCPoint>(dc_model->m_DCModel_info.m_ldc_point_num);
			dc_model->b_ldc_point_free = ctx.m_storage_memory.allocateMemory<int32_t>(dc_model->m_DCModel_info.m_ldc_point_num);

			dc_model->b_dc_vertex = ctx.m_storage_memory.allocateMemory<u8vec4>(256 * 256 * 256);
			dc_model->b_dc_index_counter = ctx.m_storage_memory.allocateMemory<vk::DrawIndirectCommand>(1);
			dc_model->b_dc_index = ctx.m_storage_memory.allocateMemory<u8vec4>(256 * 256 * 256);

			vk::DescriptorBufferInfo uniforms[] =
			{
				dc_model->u_DCModel_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] =
			{
				dc_model->b_ldc_counter.getInfo(),
				dc_model->b_ldc_point_link_head.getInfo(),
				dc_model->b_ldc_point_link_next.getInfo(),
				dc_model->b_ldc_point.getInfo(),
				dc_model->b_ldc_point_free.getInfo(),
				dc_model->b_dc_vertex.getInfo(),
				dc_model->b_dc_index_counter.getInfo(),
				dc_model->b_dc_index.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(dc_model->m_DS_DC.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(1)
				.setDstSet(dc_model->m_DS_DC.get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		{
			cmd.fillBuffer(dc_model->b_ldc_counter.getInfo().buffer, dc_model->b_ldc_counter.getInfo().offset, dc_model->b_ldc_counter.getInfo().range, 0);
			cmd.fillBuffer(dc_model->b_ldc_point_link_head.getInfo().buffer, dc_model->b_ldc_point_link_head.getInfo().offset, dc_model->b_ldc_point_link_head.getInfo().range, -1);
			cmd.fillBuffer(dc_model->b_ldc_point_free.getInfo().buffer, dc_model->b_ldc_point_free.getInfo().offset, dc_model->b_ldc_point_free.getInfo().range, -1);
			cmd.updateBuffer<Info>(dc_model->u_DCModel_info.getInfo().buffer, dc_model->u_DCModel_info.getInfo().offset, dc_model->m_DCModel_info);
			vk::BufferMemoryBarrier barrier[] =
			{
				dc_model->u_DCModel_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				dc_model->b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				dc_model->b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				dc_model->b_ldc_point_free.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
		}

		return dc_model;
	}

	static void CreateLDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model, Model& model);

	static void CreateDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model);

};

void DCModel::CreateLDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model, Model& model)
{
	{
		cmd.fillBuffer(dc_model.b_ldc_counter.getInfo().buffer, dc_model.b_ldc_counter.getInfo().offset, dc_model.b_ldc_counter.getInfo().range, 0);
		cmd.fillBuffer(dc_model.b_ldc_point_link_head.getInfo().buffer, dc_model.b_ldc_point_link_head.getInfo().offset, dc_model.b_ldc_point_link_head.getInfo().range, -1);
		cmd.fillBuffer(dc_model.b_ldc_point_free.getInfo().buffer, dc_model.b_ldc_point_free.getInfo().offset, dc_model.b_ldc_point_free.getInfo().range, -1);
		vk::BufferMemoryBarrier barrier[] =
		{
			dc_model.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point_free.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
	}

	cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_LDC_boolean_add.get());
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeLDC.get(), 0, { model.m_DS_Model.get() }, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeLDC.get(), 1, { dc_model.m_DS_DC.get() }, {});

	auto num = app::calcDipatchGroups(LDC_Reso, uvec3(64, 1, 1));
	cmd.dispatch(num.x, num.y, num.z);

}

void DCModel::CreateDCModel(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& dc_model)
{
	DebugLabel _label(cmd, __FUNCTION__);

	{
		vk::BufferMemoryBarrier barrier[] =
		{
//			dc_ctx.b_dc_cell_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

	}

	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 0, { dc_model.m_DS_DC.get() }, {});
	cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipelinelayout_MakeDC.get(), 1, { dc_ctx.m_DS_worker.get() }, {});

	_label.insert("Make DC Cell");
	{

		cmd.fillBuffer(dc_ctx.b_dc_cell.getInfo().buffer, dc_ctx.b_dc_cell.getInfo().offset, dc_ctx.b_dc_cell.getInfo().range, 0);
		cmd.fillBuffer(dc_ctx.b_dc_cell_hashmap.getInfo().buffer, dc_ctx.b_dc_cell_hashmap.getInfo().offset, dc_ctx.b_dc_cell_hashmap.getInfo().range, 0);

		cmd.updateBuffer<vk::DrawIndirectCommand>(dc_model.b_dc_index_counter.getInfo().buffer, dc_model.b_dc_index_counter.getInfo().offset, { vk::DrawIndirectCommand(0,1,0,0) });

		vk::BufferMemoryBarrier barrier[] =
		{
			dc_ctx.b_dc_cell.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_MakeDCCell.get());

		auto num = app::calcDipatchGroups(LDC_Reso, uvec3(64, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	_label.insert("Make DC Vertex");
	{

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_MakeDCVertex.get());

		auto num = app::calcDipatchGroups(Voxel_Reso, uvec3(64, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	_label.insert("Make DC Face");
	{

		vk::BufferMemoryBarrier barrier[] =
		{
			dc_model.b_dc_vertex.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_dc_index_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, dc_ctx.m_pipeline_MakeDCFace.get());

		auto num = app::calcDipatchGroups(Voxel_Reso, uvec3(64, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);

	}

	{
		vk::BufferMemoryBarrier barrier[] =
		{
			dc_model.b_dc_index_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { array_size(barrier), barrier }, {});

	}
}

struct ModelDrawParam
{
	vec4 axis[3];
	vec4 min;
	vec4 pos;
};

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

			vk::PushConstantRange constants[] = {
				vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setOffset(0).setSize(sizeof(ModelDrawParam)),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			pipeline_layout_info.setPPushConstantRanges(constants);
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

	void executClear(vk::CommandBuffer& cmd, DCModel& dc_model)
	{
		DebugLabel _label(cmd, __FUNCTION__);
		{
			vk::BufferMemoryBarrier barrier[] =
			{
				dc_model.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				dc_model.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				dc_model.b_ldc_point_free.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_size(barrier), barrier }, {});

		}
		cmd.fillBuffer(dc_model.b_ldc_counter.getInfo().buffer, dc_model.b_ldc_counter.getInfo().offset, dc_model.b_ldc_counter.getInfo().range, 0);
		cmd.fillBuffer(dc_model.b_ldc_point_link_head.getInfo().buffer, dc_model.b_ldc_point_link_head.getInfo().offset, dc_model.b_ldc_point_link_head.getInfo().range, -1);
		cmd.fillBuffer(dc_model.b_ldc_point_free.getInfo().buffer, dc_model.b_ldc_point_free.getInfo().offset, dc_model.b_ldc_point_free.getInfo().range, -1);
		vk::BufferMemoryBarrier barrier[] =
		{
			dc_model.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			dc_model.b_ldc_point_free.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

	}


	void executeBooleanAdd(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& base, Model& boolean, const ModelInstance& instance)
	{
		DebugLabel _label(cmd, __FUNCTION__);
		{
			vk::BufferMemoryBarrier barrier[] =
			{
				base.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				base.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				base.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Boolean_Add].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 0, { base.m_DS_DC.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 1, { boolean.m_DS_Model.get() }, {});
		{
			vec3 axis[] = { vec3(1., 0., 0.), vec3(0., 1., 0.),vec3(0., 0., 1.) };
			vec3 dir = normalize(instance.dir.xyz());
			auto rot = quat(axis[2], dir);

			vec3 f = (rot * axis[2]);
			vec3 s = (rot * axis[0]);
			vec3 u = (rot * axis[1]);

			vec3 min = boolean.m_info.m_aabb_min;
			vec3 max = boolean.m_info.m_aabb_max;

			vec3 extent = max - min;
			vec3 center = (extent - min) * 0.5;
			vec3 new_min = rot * (min - center) + center;

			ModelDrawParam param;
			param.axis[0] = vec4(s, 0.f);
			param.axis[1] = vec4(u, 0.f);
			param.axis[2] = vec4(f, 0.f);
			param.min = vec4(new_min, 0.f);

			param.pos = vec4(instance.pos.xyz(), 0.f);

			cmd.pushConstants<ModelDrawParam>(m_PL_boolean.get(), vk::ShaderStageFlagBits::eCompute, 0, param);


			// 無駄なレイを飛ばさないようにカリング
//			auto num = app::calcDipatchGroups(LDC_Reso, uvec3(8, 8, 1));
//			cmd.dispatch(num.x, num.y, 3);
			{
				vec3 pminmax[] = { min, max };
				vec3 rmin = center;
				vec3 rmax = center;
				for (int z = 0; z < 2; z++)
				for (int y = 0; y < 2; y++)
				for (int x = 0; x < 2; x++)
				{
					vec3 p = vec3(pminmax[x == 0].x, pminmax[y == 0].y, pminmax[z == 0].z);
					vec3 new_p = rot * (p - center) + center;
					rmin = glm::min(new_p, rmin);
					rmax = glm::max(new_p, rmax);
				}

				vec3 cell_size = (Voxel_Block_Size / vec3(LDC_Reso)) * 8;

				ivec3 base = floor((rmin + instance.pos.xyz()) / cell_size.xxx);
				ivec3 count = ceil((rmax - rmin + instance.pos.xyz()) / cell_size.xxx);

				count = clamp(clamp(count + base, 0, 32) - base, 0, 32);
				base = clamp(base, 0, 32);

				cmd.dispatchBase(base.y, base.z, 0, count.y, count.z, 1);
				cmd.dispatchBase(base.z, base.x, 1, count.z, count.x, 1);
				cmd.dispatchBase(base.x, base.y, 2, count.x, count.y, 1);
			}

		}



	}
	void executeBooleanSub(vk::CommandBuffer& cmd, DCContext& dc_ctx, DCModel& base, Model& boolean, const ModelInstance& instance)
	{
		DebugLabel _label(cmd, __FUNCTION__);
		{

			vk::BufferMemoryBarrier barrier[] =
			{
				base.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				base.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				base.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

		}
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_Boolean_Sub].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 0, { base.m_DS_DC.get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL_boolean.get(), 1, { boolean.m_DS_Model.get() }, {});
		cmd.pushConstants<ModelInstance>(m_PL_boolean.get(), vk::ShaderStageFlagBits::eCompute, 0, instance);

		auto num = app::calcDipatchGroups(LDC_Reso, uvec3(64, 1, 1));
		cmd.dispatch(num.x, num.y, num.z);

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
				assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

				// viewport
				vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt.m_resolution.width, (float)rt.m_resolution.height, 0.f, 1.f);
				vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_resolution.width, rt.m_resolution.height));
				vk::PipelineViewportStateCreateInfo viewportInfo;
				viewportInfo.setViewportCount(1);
				viewportInfo.setPViewports(&viewport);
				viewportInfo.setScissorCount(1);
				viewportInfo.setPScissors(&scissor);


				vk::PipelineRasterizationStateCreateInfo rasterization_info;
				rasterization_info.setPolygonMode(vk::PolygonMode::eLine);
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
		DebugLabel _label(cmd, __FUNCTION__);
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
		cmd.draw(36, 1, 0, 0);

		cmd.endRenderPass();

	}
	void ExecuteRenderDCModel(vk::CommandBuffer cmd, DCContext& dc_ctx, DCModel& ldc_model, RenderTarget& rt)
	{
		DebugLabel _label(cmd, __FUNCTION__);
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
		DebugLabel _label(cmd, __FUNCTION__);
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

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(50.f, 50.f, -500.f);
	camera->getData().m_target = glm::vec3(256.f);
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
	DCFunctionLibrary dc_fl(*context, *dc_ctx);

	auto model_box = Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Box.dae", { 100.f });
//	auto model = Model::LoadModel(*context, *dc_ctx, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\Duck.dae", { 1.f });


	auto dc_model = DCModel::Construct(*context, *dc_ctx);
	Renderer renderer(*context, *dc_ctx, *app.m_window->getFrontBuffer());

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		dc_fl.executeBooleanAdd(cmd, *dc_ctx, *dc_model, *model_box, { vec4(100.f), vec4(0.f, 0.f, 1.f, 0.f) });
		DCModel::CreateDCModel(cmd, *dc_ctx, *dc_model);

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

				{
					struct I
					{
						float time;
						vec3 rot[2];
						vec3 pos[2];
					};
					static I instance{ 0.f, {glm::ballRand(1.f), glm::ballRand(1.f) }, {glm::linearRand(vec3(0.f), vec3(500.f)), glm::linearRand(vec3(0.f), vec3(500.f))} };
					instance.time += 0.005f;
					if (instance.time >= 1.f)
					{
						instance.time -= 1.f;
						instance.rot[0] = instance.rot[1];
						instance.rot[1] = glm::ballRand(1.f);
						instance.pos[0] = instance.pos[1];
						instance.pos[1] = glm::linearRand(vec3(0.f), vec3(500.f));
					}
					ModelInstance model_instance{ vec4(mix(instance.pos[0], instance.pos[1], instance.time), 100.f), vec4(mix(instance.rot[0], instance.rot[1], instance.time), 0.f) };
//					ModelInstance model_instance{ vec4(100.f), vec4(mix(instance.rot[0], instance.rot[1], instance.time), 0.f) };
//					for (int i = 0; i < 1000; i++)
					{
						dc_fl.executClear(cmd, *dc_model);
//						dc_fl.executeBooleanAdd(cmd, *dc_ctx, *dc_model, *model_box, model_instance);
						dc_fl.executeBooleanAdd(cmd, *dc_ctx, *dc_model, *model_box, { vec4(100.f), vec4(0.f, 0.f, 1.f, 0.f) });
					}

					DCModel::CreateDCModel(cmd, *dc_ctx, *dc_model);

				}
				renderer.ExecuteTestRender(cmd, *dc_ctx, *dc_model, *app.m_window->getFrontBuffer());
				renderer.ExecuteRenderDCModel(cmd, *dc_ctx, *dc_model, *app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.3fms\n", time.getElapsedTimeAsMilliSeconds());
	}
}