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
#include <btrlib/cModel.h>

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


struct Context
{
	enum DSL
	{
		DSL_Model,
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
	btr::BufferMemoryEx<vk::DrawIndexedIndirectCommand> b_draw_cmd;

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

			model->b_draw_cmd = ctx.m_ctx->m_storage_memory.allocateMemory<vk::DrawIndexedIndirectCommand>(1);
			cmd.updateBuffer<vk::DrawIndexedIndirectCommand>(model->b_draw_cmd.getInfo().buffer, model->b_draw_cmd.getInfo().offset, vk::DrawIndexedIndirectCommand(numIndex, 1, 0, 0, 0));
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

struct ModelRenderer
{
	enum
	{
		DSL_Renderer,
		DSL_Num,
	};
	enum
	{
		DS_Renderer,
		DS_Num,
	};

	enum
	{
		Pipeline_Render,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_Render,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	vk::UniqueRenderPass m_RP_render;
	vk::UniqueFramebuffer m_FB_render;

	ModelRenderer(Context& ctx, RenderTarget& rt)
	{
		auto cmd = ctx.m_ctx->m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
// 		{
// 			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
// 			vk::DescriptorSetLayoutBinding binding[] = {
// 				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eUniformBuffer, 1, stage),
// 			};
// 			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
// 			desc_layout_info.setBindingCount(array_length(binding));
// 			desc_layout_info.setPBindings(binding);
// 			m_DSL[DSL_Renderer] = ctx.m_ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
// 		}

		// descriptor set
		{
// 			vk::DescriptorSetLayout layouts[] =
// 			{
// 				m_DSL[DSL_Renderer].get(),
// 			};
// 			vk::DescriptorSetAllocateInfo desc_info;
// 			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
// 			desc_info.setDescriptorSetCount(array_length(layouts));
// 			desc_info.setPSetLayouts(layouts);
// 			m_DS[DS_Renderer] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
//
// 			vk::DescriptorBufferInfo uniforms[] =
// 			{
// 				u_info.getInfo(),
// 			};
// 			vk::DescriptorBufferInfo storages[] =
// 			{
// 				b_hashmap.getInfo(),
// 				b_hashmap_mask.getInfo(),
// 				b_interior_counter.getInfo(),
// 				b_leaf_counter.getInfo(),
// 				b_interior.getInfo(),
// 				b_leaf.getInfo(),
// 			};
// 
// 			vk::WriteDescriptorSet write[] =
// 			{
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
// 				.setDescriptorCount(array_length(uniforms))
// 				.setPBufferInfo(uniforms)
// 				.setDstBinding(0)
// 				.setDstSet(m_DS[DS_Renderer].get()),
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 				.setDescriptorCount(array_length(storages))
// 				.setPBufferInfo(storages)
// 				.setDstBinding(1)
// 				.setDstSet(m_DS[DS_Renderer].get()),
// 			};
// 			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
//					m_DSL[DSL_Renderer].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_Render] = ctx.m_ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
// 			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
// 			{
// 				{"Voxel_AllocateTopChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 				{"Voxel_AllocateMidChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 				{"Voxel_MakeHashMapMask.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 				{"Voxel_Rendering.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 			};
// 			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
// 			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
// 			for (size_t i = 0; i < array_length(shader_param); i++)
// 			{
// 				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
// 				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
// 			}
// 
// 			vk::ComputePipelineCreateInfo compute_pipeline_info[] =
// 			{
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[0])
// 				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[1])
// 				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[2])
// 				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[3])
// 				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
// 			};
// 			m_pipeline[Pipeline_MakeVoxelTopChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
// 			m_pipeline[Pipeline_MakeVoxelMidChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
// 			m_pipeline[Pipeline_MakeHashMapMask] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
// 			m_pipeline[Pipeline_RenderVoxel] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
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

				m_RP_render = ctx.m_ctx->m_device.createRenderPassUnique(renderpass_info);

				{
					vk::FramebufferCreateInfo framebuffer_info;
					framebuffer_info.setRenderPass(m_RP_render.get());
					framebuffer_info.setWidth(rt.m_resolution.width);
					framebuffer_info.setHeight(rt.m_resolution.height);
					framebuffer_info.setLayers(1);

					m_FB_render = ctx.m_ctx->m_device.createFramebufferUnique(framebuffer_info);
				}
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"ModelRender.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"ModelRender.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
				vk::Viewport(0.f, 0.f, rt.m_resolution.width, rt.m_resolution.height, 0.f, 1.f),
			};
			vk::Rect2D scissor[] =
			{
				vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution),
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
				.setLayout(m_PL[PipelineLayout_Render].get())
				.setRenderPass(m_RP_render.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline[Pipeline_Render] = ctx.m_ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}

	}

	void execute_Render(vk::CommandBuffer cmd, RenderTarget& rt, Model& model)
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

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Render].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { m_DS[DSL_Renderer].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 1, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 2, { rt.m_descriptor.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_RP_render.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_FB_render.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		vk::Buffer buffers[] = { model.b_vertex.getInfo().buffer };
		vk::DeviceSize offsets[] = { model.b_vertex.getInfo().offset };
		cmd.bindVertexBuffers(0, 1, buffers, offsets);
		cmd.drawIndexedIndirect(model.b_draw_cmd.getInfo().buffer, model.b_draw_cmd.getInfo().offset, 1, sizeof(vk::DrawIndexedIndirectCommand));

		cmd.endRenderPass();

	}
};



int main()
{


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
	auto model_ctx = std::make_shared<ModelContext>(*context);

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

//	std::shared_ptr<Model> model = Model::LoadModel(*ctx, btr::getResourceAppPath() + "Duck.dae");
	cModel model;
	model.load(context, *model_ctx, btr::getResourceAppPath() + "pbr/DamagedHelmet.gltf");
//	std::shared_ptr<Model> model = cModel::LoadModel(*ctx, btr::getResourceAppPath() + "Duck.dae");
	app.setup();

	ModelRenderer renderer(*ctx, *app.m_window->getFrontBuffer());
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
//					renderer.execute_Render(cmd, *app.m_window->getFrontBuffer(), *model);
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

