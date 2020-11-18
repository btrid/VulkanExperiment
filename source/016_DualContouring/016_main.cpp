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

	uint m_vertex_num;

	static std::shared_ptr<Model> LoadModel(const std::shared_ptr<btr::Context>& context, const std::string& filename)
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


		auto vertex = context->m_staging_memory.allocateMemory(numVertex * sizeof(aiVector3D), true);
		auto index = context->m_staging_memory.allocateMemory(numIndex * sizeof(uint32_t), true);

		uint32_t index_offset = 0;
		uint32_t vertex_offset = 0;
		for (size_t i = 0; i < scene->mNumMeshes; i++)
		{
			aiMesh* mesh = scene->mMeshes[i];
			for (u32 n = 0; n < mesh->mNumFaces; n++) 
			{
				std::copy(mesh->mFaces[n].mIndices, mesh->mFaces[n].mIndices + 3, index.getMappedPtr<uint32_t>(index_offset));
				index_offset += 3;
			}
			//		std::copy(mesh->mVertices, mesh->mVertices + mesh->mNumVertices, std::back_inserter(vertex));
			std::copy(mesh->mVertices, mesh->mVertices + mesh->mNumVertices, vertex.getMappedPtr<aiVector3D>(vertex_offset));
			vertex_offset += mesh->mNumVertices;
		}

		importer.FreeScene();

		std::shared_ptr<Model> model = std::make_shared<Model>();
		model->m_vertex = context->m_vertex_memory.allocateMemory(numVertex * sizeof(aiVector3D));
		model->m_index = context->m_vertex_memory.allocateMemory(numIndex * sizeof(uint32_t));

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		std::array<vk::BufferCopy, 2> copy;
		copy[0].srcOffset = vertex.getInfo().offset;
		copy[0].dstOffset = model->m_vertex.getInfo().offset;
		copy[0].size = vertex.getInfo().range;
		copy[1].srcOffset = index.getInfo().offset;
		copy[1].dstOffset = model->m_index.getInfo().offset;
		copy[1].size = index.getInfo().range;
		cmd.copyBuffer(vertex.getInfo().buffer, model->m_vertex.getInfo().buffer, copy);

		vk::BufferMemoryBarrier barrier[] = {
			model->m_vertex.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->m_index.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});
		model->m_vertex_num = numVertex;

		return model;
	}

};

struct RTModel
{
//	RTModel(const RTModel&) = delete;
//	RTModel& operater(RTModel&) = delete;

	struct RayTracingScratchBuffer
	{
		uint64_t deviceAddress = 0;
		vk::UniqueBuffer buffer;
		vk::UniqueDeviceMemory memory;
	};

	// Holds data for a memory object bound to an acceleration structure
	struct RayTracingObjectMemory
	{
		uint64_t deviceAddress;
		vk::UniqueDeviceMemory memory;
	};

	// Ray tracing acceleration structure
	struct AccelerationStructure 
	{
		vk::UniqueAccelerationStructureKHR accelerationStructure;
		uint64_t handle;
		RayTracingObjectMemory objectMemory;
	};

	AccelerationStructure m_bottomLevelAS;
	AccelerationStructure m_topLevelAS;

	static RayTracingObjectMemory createObjectMemory(std::shared_ptr<btr::Context>& ctx, vk::AccelerationStructureKHR& acceleration_structure)
	{
		RayTracingObjectMemory objectMemory{};

		vk::AccelerationStructureMemoryRequirementsInfoKHR accelerationStructureMemoryRequirements{};
		accelerationStructureMemoryRequirements.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eObject;
		accelerationStructureMemoryRequirements.buildType = vk::AccelerationStructureBuildTypeKHR::eDevice;
		accelerationStructureMemoryRequirements.accelerationStructure = acceleration_structure;
		vk::MemoryRequirements2 memoryRequirements2 = ctx->m_device.getAccelerationStructureMemoryRequirementsKHR(accelerationStructureMemoryRequirements);

		VkMemoryRequirements memoryRequirements = memoryRequirements2.memoryRequirements;

		vk::MemoryAllocateInfo memoryAI;
		memoryAI.allocationSize = memoryRequirements.size;
		memoryAI.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx->m_physical_device, memoryRequirements, vk::MemoryPropertyFlagBits::eDeviceLocal);
		objectMemory.memory = ctx->m_device.allocateMemoryUnique(memoryAI);

		return objectMemory;
	}
	//	Create a scratch buffer to hold temporary data for a ray tracing acceleration structure
	static RayTracingScratchBuffer createScratchBuffer(std::shared_ptr<btr::Context>& ctx, vk::AccelerationStructureKHR& AS)
	{
		RayTracingScratchBuffer scratchBuffer{};

		vk::AccelerationStructureMemoryRequirementsInfoKHR accelerationStructureMemoryRequirements;
		accelerationStructureMemoryRequirements.type = vk::AccelerationStructureMemoryRequirementsTypeKHR::eBuildScratch;
		accelerationStructureMemoryRequirements.buildType = vk::AccelerationStructureBuildTypeKHR::eDevice;
		accelerationStructureMemoryRequirements.accelerationStructure = AS;
		VkMemoryRequirements2 memoryRequirements2 = ctx->m_device.getAccelerationStructureMemoryRequirementsKHR(accelerationStructureMemoryRequirements);

		vk::BufferCreateInfo bufferCI{};
		bufferCI.size = memoryRequirements2.memoryRequirements.size;
		bufferCI.usage = vk::BufferUsageFlagBits::eRayTracingKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress;
		bufferCI.sharingMode = vk::SharingMode::eExclusive;
		scratchBuffer.buffer = ctx->m_device.createBufferUnique(bufferCI);

		vk::MemoryRequirements memoryRequirements = ctx->m_device.getBufferMemoryRequirements(scratchBuffer.buffer.get());

		vk::MemoryAllocateFlagsInfo memoryAllocateFI;
		memoryAllocateFI.flags = vk::MemoryAllocateFlagBits::eDeviceAddress;

		vk::MemoryAllocateInfo memoryAI;
		memoryAI.pNext = &memoryAllocateFI;
		memoryAI.allocationSize = memoryRequirements.size;
		memoryAI.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx->m_physical_device, memoryRequirements, vk::MemoryPropertyFlagBits::eDeviceLocal);
		scratchBuffer.memory = ctx->m_device.allocateMemoryUnique(memoryAI);
		ctx->m_device.bindBufferMemory(scratchBuffer.buffer.get(), scratchBuffer.memory.get(), 0);

		vk::BufferDeviceAddressInfoKHR buffer_device_address_info;
		buffer_device_address_info.buffer = scratchBuffer.buffer.get();
		scratchBuffer.deviceAddress = ctx->m_device.getBufferAddress(buffer_device_address_info);

		return scratchBuffer;
	}

	static std::shared_ptr<RTModel> Construct(std::shared_ptr<btr::Context>& ctx, Model& model)
	{
		auto cmd = ctx->m_cmd_pool->allocCmdTempolary(0);

		AccelerationStructure bottomLevelAS;
		{

			vk::AccelerationStructureCreateGeometryTypeInfoKHR accelerationCreateGeometryInfo{};
			accelerationCreateGeometryInfo.geometryType = vk::GeometryTypeKHR::eTriangles;
			accelerationCreateGeometryInfo.maxPrimitiveCount = 1;
			accelerationCreateGeometryInfo.indexType = vk::IndexType::eUint32;
			accelerationCreateGeometryInfo.maxVertexCount = model.m_vertex_num;
			accelerationCreateGeometryInfo.vertexFormat = vk::Format::eR32G32B32Sfloat;
			accelerationCreateGeometryInfo.allowsTransforms = VK_FALSE;

			vk::AccelerationStructureCreateInfoKHR accelerationCI;
			accelerationCI.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
			accelerationCI.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
			accelerationCI.maxGeometryCount = 1;
			accelerationCI.pGeometryInfos = &accelerationCreateGeometryInfo;

			bottomLevelAS.accelerationStructure = ctx->m_device.createAccelerationStructureKHRUnique(accelerationCI);

			// Bind object memory to the top level acceleration structure
			bottomLevelAS.objectMemory = createObjectMemory(ctx, bottomLevelAS.accelerationStructure.get());

			vk::BindAccelerationStructureMemoryInfoKHR bindAccelerationMemoryInfo;
			bindAccelerationMemoryInfo.accelerationStructure = bottomLevelAS.accelerationStructure.get();
			bindAccelerationMemoryInfo.memory = bottomLevelAS.objectMemory.memory.get();
			ctx->m_device.bindAccelerationStructureMemoryKHR(1, &bindAccelerationMemoryInfo);

			std::array<vk::AccelerationStructureGeometryKHR, 1> accelerationStructureGeometry;
			accelerationStructureGeometry[0].flags = vk::GeometryFlagBitsKHR::eOpaque;
			accelerationStructureGeometry[0].geometryType = vk::GeometryTypeKHR::eTriangles;
			accelerationStructureGeometry[0].geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
			accelerationStructureGeometry[0].geometry.triangles.vertexData.deviceAddress = ctx->m_device.getBufferAddress(vk::BufferDeviceAddressInfoKHR().setBuffer(model.m_vertex.getInfo().buffer)) + model.m_vertex.getInfo().offset;
			accelerationStructureGeometry[0].geometry.triangles.vertexStride = sizeof(aiVector3D);
			accelerationStructureGeometry[0].geometry.triangles.indexType = vk::IndexType::eUint32;
			accelerationStructureGeometry[0].geometry.triangles.indexData.deviceAddress = ctx->m_device.getBufferAddress(vk::BufferDeviceAddressInfoKHR().setBuffer(model.m_index.getInfo().buffer)) + model.m_index.getInfo().offset;

			vk::AccelerationStructureGeometryKHR* acceleration_structure_geometries = accelerationStructureGeometry.data();

			// Create a small scratch buffer used during build of the bottom level acceleration structure
			auto scratchBuffer = createScratchBuffer(ctx, bottomLevelAS.accelerationStructure.get());

			vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo;
			accelerationBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
			accelerationBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
			accelerationBuildGeometryInfo.update = VK_FALSE;
			accelerationBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS.accelerationStructure.get();
			accelerationBuildGeometryInfo.geometryArrayOfPointers = VK_FALSE;
			accelerationBuildGeometryInfo.geometryCount = 1;
			accelerationBuildGeometryInfo.ppGeometries = &acceleration_structure_geometries;
			accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

			vk::AccelerationStructureBuildOffsetInfoKHR accelerationBuildOffsetInfo{};
			accelerationBuildOffsetInfo.primitiveCount = 1;
			accelerationBuildOffsetInfo.primitiveOffset = 0x0;
			accelerationBuildOffsetInfo.firstVertex = 0;
			accelerationBuildOffsetInfo.transformOffset = 0x0;

			std::vector<const vk::AccelerationStructureBuildOffsetInfoKHR*> offset = { &accelerationBuildOffsetInfo };
//			if (rayTracingFeatures.rayTracingHostAccelerationStructureCommands)
			{
// 				// Implementation supports building acceleration structure building on host
//				ctx->m_device.buildAccelerationStructureKHR(1, &accelerationBuildGeometryInfo, offset.data());
			}
//			else
			{
				// Acceleration structure needs to be build on the device
				cmd.buildAccelerationStructureKHR({ accelerationBuildGeometryInfo }, offset);
			}

			vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{};
			accelerationDeviceAddressInfo.accelerationStructure = bottomLevelAS.accelerationStructure.get();

//			bottomLevelAS.handle = ctx->m_device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);
			sDeleter::Order().enque(std::move(scratchBuffer));

		}
		AccelerationStructure topLevelAS;
		{
			vk::AccelerationStructureCreateGeometryTypeInfoKHR accelerationCreateGeometryInfo;
			accelerationCreateGeometryInfo.geometryType = vk::GeometryTypeKHR::eInstances;
			accelerationCreateGeometryInfo.maxPrimitiveCount = 1;
			accelerationCreateGeometryInfo.allowsTransforms = VK_FALSE;

			vk::AccelerationStructureCreateInfoKHR accelerationCI;
			accelerationCI.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
			accelerationCI.maxGeometryCount = 1;
			accelerationCI.pGeometryInfos = &accelerationCreateGeometryInfo;
			topLevelAS.accelerationStructure = ctx->m_device.createAccelerationStructureKHRUnique(accelerationCI);

			// Bind object memory to the top level acceleration structure
			topLevelAS.objectMemory = createObjectMemory(ctx, topLevelAS.accelerationStructure.get());

			vk::BindAccelerationStructureMemoryInfoKHR bindAccelerationMemoryInfo;
			bindAccelerationMemoryInfo.accelerationStructure = topLevelAS.accelerationStructure.get();
			bindAccelerationMemoryInfo.memory = topLevelAS.objectMemory.memory.get();
			ctx->m_device.bindAccelerationStructureMemoryKHR({ bindAccelerationMemoryInfo });

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
			instance.accelerationStructureReference = bottomLevelAS.handle;

			// Buffer for instance data
			btr::AllocatedMemory instance_memory;
			instance_memory.setup(ctx->m_physical_device, ctx->m_device, vk::BufferUsageFlagBits::eShaderDeviceAddress| vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal, sizeof(instance));
			auto instance_buffer = instance_memory.allocateMemory(sizeof(instance));

			cmd.updateBuffer<vk::AccelerationStructureInstanceKHR>(instance_buffer.getInfo().buffer, instance_buffer.getInfo().offset, {instance});


			vk::DeviceOrHostAddressConstKHR instance_data_device_address;
			instance_data_device_address.deviceAddress = ctx->m_device.getBufferAddress(vk::BufferDeviceAddressInfoKHR().setBuffer(instance_buffer.getInfo().buffer));

			vk::AccelerationStructureGeometryKHR accelerationStructureGeometry;
			accelerationStructureGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque;
			accelerationStructureGeometry.geometryType = vk::GeometryTypeKHR::eInstances;
			accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
			accelerationStructureGeometry.geometry.instances.data.deviceAddress = instance_data_device_address.deviceAddress;

			std::vector<vk::AccelerationStructureGeometryKHR> acceleration_geometries = { accelerationStructureGeometry };
			vk::AccelerationStructureGeometryKHR* acceleration_structure_geometries = acceleration_geometries.data();

			// Create a small scratch buffer used during build of the top level acceleration structure
			RayTracingScratchBuffer scratchBuffer = createScratchBuffer(ctx, topLevelAS.accelerationStructure.get());

			vk::AccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo;
			accelerationBuildGeometryInfo.type = vk::AccelerationStructureTypeKHR::eTopLevel;
			accelerationBuildGeometryInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
			accelerationBuildGeometryInfo.update = VK_FALSE;
//			accelerationBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
			accelerationBuildGeometryInfo.dstAccelerationStructure = topLevelAS.accelerationStructure.get();
			accelerationBuildGeometryInfo.geometryArrayOfPointers = VK_FALSE;
			accelerationBuildGeometryInfo.geometryCount = 1;
			accelerationBuildGeometryInfo.ppGeometries = &acceleration_structure_geometries;
			accelerationBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

			vk::AccelerationStructureBuildOffsetInfoKHR accelerationBuildOffsetInfo;
			accelerationBuildOffsetInfo.primitiveCount = 1;
			accelerationBuildOffsetInfo.primitiveOffset = 0x0;
			accelerationBuildOffsetInfo.firstVertex = 0;
			accelerationBuildOffsetInfo.transformOffset = 0x0;

			std::vector<const vk::AccelerationStructureBuildOffsetInfoKHR*> offset = { &accelerationBuildOffsetInfo };
// 			if (rayTracingFeatures.rayTracingHostAccelerationStructureCommands)
// 			{
// 				// Implementation supports building acceleration structure building on host
//				ctx->m_device.buildAccelerationStructureKHR({ accelerationBuildGeometryInfo }, offset);
// 			}
// 			else
			{
				// Acceleration structure needs to be build on the device
				cmd.buildAccelerationStructureKHR({ accelerationBuildGeometryInfo }, offset);
			}

			vk::AccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo;
			accelerationDeviceAddressInfo.accelerationStructure = topLevelAS.accelerationStructure.get();

//			topLevelAS.handle = ctx->m_device.getAccelerationStructureAddressKHR(accelerationDeviceAddressInfo);

			sDeleter::Order().enque(std::move(scratchBuffer), std::move(instance_buffer), std::move(instance_memory));
		}

		auto rtmodel = std::make_shared<RTModel>();
		rtmodel->m_topLevelAS = std::move(topLevelAS);
		rtmodel->m_bottomLevelAS = std::move(bottomLevelAS);

		return rtmodel;
	}


};
struct Renderer
{
	void render(vk::CommandBuffer& cmd)
	{

	}
};

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

	auto model = Model::LoadModel(context, "C:\\Users\\logos\\source\\repos\\VulkanExperiment\\resource\\tiny.x");
	auto rtmodel = RTModel::Construct(context, *model);
	{

	}
	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

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