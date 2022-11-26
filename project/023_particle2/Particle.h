#pragma once

#include <btrlib/Define.h>
#include <btrlib/Context.h>

namespace kira
{

struct Context
{
	enum DSL
	{
		DSL_Data,
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
				m_DSL[DSL_Data] = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}

		}

	}
};
struct Data
{
	vk::UniqueDescriptorSet m_DS;

	struct Info
	{

	};
	struct Manager
	{
		int32_t m_enum;
		int32_t m_pnum;
		int32_t m_mnum;
	};
	struct PChunk
	{
		int32_t m_mchunk[32];
	};
	struct EChunk
	{
		int32_t m_pchunk_begin;
		int32_t m_pchunk_end;
	};

	Info m_info;
	btr::BufferMemoryEx<Info> u_info;
	btr::BufferMemoryEx<Manager> b_manager;
	btr::BufferMemoryEx<vec4> b_mchunk;
	btr::BufferMemoryEx<vec4> b_pchunk;
	btr::BufferMemoryEx<vec4> b_echunk;
	btr::BufferMemoryEx<uvec4> b_update_cmd;


	static std::shared_ptr<Data> Make(Context& ctx)
	{
		auto data = std::make_shared<Data>();
		// descriptor set
		{
			vk::DescriptorSetLayout layouts[] =
			{
				ctx.m_DSL[Context::DSL_Data].get()
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			data->m_DS = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] =
			{
				data->u_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] =
			{
				data->b_manager.getInfo(),
				data->b_mchunk.getInfo(),
				data->b_pchunk.getInfo(),
				data->b_echunk.getInfo(),
				data->b_update_cmd.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(data->m_DS.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(1)
				.setDstSet(data->m_DS.get()),
			};
			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

	}

};

struct Renderer
{
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
		Pipeline_Sprite,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_Sprite,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	Renderer(Context& ctx, RenderTarget& rt)
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
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL[PipelineLayout_Sprite] = ctx.m_ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		// descriptor set
		{

			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[PipelineLayout_Sprite].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[Pipeline_Sprite] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

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
// 				.setDstSet(m_DS[DS_Voxel].get()),
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 				.setDescriptorCount(array_length(storages))
// 				.setPBufferInfo(storages)
// 				.setDstBinding(1)
// 				.setDstSet(m_DS[DS_Voxel].get()),
// 			};
// 			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}


// 		// compute pipeline
// 		{
// 			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
// 			{
// 				{"VoxelMakeTD_AllocateTopChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 				{"VoxelMakeTD_AllocateMidChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
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
// 				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
// 			};
// 			m_pipeline[Pipeline_MakeVoxelTopChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
// 			m_pipeline[Pipeline_MakeVoxelMidChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
// 			m_pipeline[Pipeline_RenderVoxel] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
// 		}

// 		// graphics pipeline render
// 		{
// 
// 			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
// 			{
// 				{"VoxelMakeTD_ModelVoxelize.vert.spv", vk::ShaderStageFlagBits::eVertex},
// 				{"VoxelMakeTD_ModelVoxelize.geom.spv", vk::ShaderStageFlagBits::eGeometry},
// 				{"VoxelMakeTD_ModelVoxelize.frag.spv", vk::ShaderStageFlagBits::eFragment},
// 
// 			};
// 			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
// 			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
// 			for (size_t i = 0; i < array_length(shader_param); i++)
// 			{
// 				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
// 				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
// 			}
// 
// 			// assembly
// 			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
// 			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
// 			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);
// 
// 			// viewport
// 			vk::Viewport viewport[] =
// 			{
// 				vk::Viewport(0.f, 0.f, VoxelReso, VoxelReso, 0.f, 1.f),
// 			};
// 			vk::Rect2D scissor[] =
// 			{
// 				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(VoxelReso, VoxelReso)),
// 			};
// 			vk::PipelineViewportStateCreateInfo viewportInfo;
// 			viewportInfo.setViewportCount(array_length(viewport));
// 			viewportInfo.setPViewports(viewport);
// 			viewportInfo.setScissorCount(array_length(scissor));
// 			viewportInfo.setPScissors(scissor);
// 
// 
// 			vk::PipelineRasterizationStateCreateInfo rasterization_info;
// 			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
// 			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
// 			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
// 			rasterization_info.setLineWidth(1.f);
// 
// 			auto prop = ctx.m_ctx->m_physical_device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceConservativeRasterizationPropertiesEXT>();
// 			auto& conservativeRasterProps = prop.get<vk::PhysicalDeviceConservativeRasterizationPropertiesEXT>();
// 			vk::PipelineRasterizationConservativeStateCreateInfoEXT conservativeRasterState_CI;
// 			conservativeRasterState_CI.conservativeRasterizationMode = vk::ConservativeRasterizationModeEXT::eOverestimate;
// 			conservativeRasterState_CI.extraPrimitiveOverestimationSize = conservativeRasterProps.maxExtraPrimitiveOverestimationSize;
// 			rasterization_info.pNext = &conservativeRasterState_CI;
// 
// 			vk::PipelineMultisampleStateCreateInfo sample_info;
// 			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);
// 
// 			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
// 			depth_stencil_info.setDepthTestEnable(VK_FALSE);
// 
// 
// 			vk::PipelineColorBlendStateCreateInfo blend_info;
// 			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
// 
// 			vk::VertexInputAttributeDescription vi_attrib;
// 			vi_attrib.binding = 0;
// 			vi_attrib.format = vk::Format::eR32G32B32Sfloat;
// 			vi_attrib.location = 0;
// 			vi_attrib.offset = 0;
// 			vk::VertexInputBindingDescription vi_binding;
// 			vi_binding.binding = 0;
// 			vi_binding.inputRate = vk::VertexInputRate::eVertex;
// 			vi_binding.stride = 12;
// 			vertex_input_info.vertexAttributeDescriptionCount = 1;
// 			vertex_input_info.pVertexAttributeDescriptions = &vi_attrib;
// 			vertex_input_info.vertexBindingDescriptionCount = 1;
// 			vertex_input_info.pVertexBindingDescriptions = &vi_binding;
// 
// 			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
// 				vk::GraphicsPipelineCreateInfo()
// 				.setStageCount(shaderStages.size())
// 				.setPStages(shaderStages.data())
// 				.setPVertexInputState(&vertex_input_info)
// 				.setPInputAssemblyState(&assembly_info)
// 				.setPViewportState(&viewportInfo)
// 				.setPRasterizationState(&rasterization_info)
// 				.setPMultisampleState(&sample_info)
// 				.setLayout(m_PL[PipelineLayout_MakeVoxel].get())
// 				.setPDepthStencilState(&depth_stencil_info)
// 				.setPColorBlendState(&blend_info);
// 
// 			vk::PipelineRenderingCreateInfoKHR pipeline_rendering_create_info = vk::PipelineRenderingCreateInfoKHR();
// 			graphics_pipeline_info.setPNext(&pipeline_rendering_create_info);
// 
// 			m_pipeline[Pipeline_MakeVoxel] = ctx.m_ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;
// 
// 		}

	}

	void execute(vk::CommandBuffer cmd)
	{

	}

	void executeDebug_RenderVoxel(vk::CommandBuffer cmd, RenderTarget& rt)
	{
	}
};

}
