#pragma once

#include <vector>
#include <queue>
#include <random>
#include <cstdint>

#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <btrlib/cStopWatch.h>

#include <applib/App.h>
#include <013_2DGI/GI2D/GI2DContext.h>


struct PathContextCPU
{
	struct Description
	{
		ivec2 m_size;
		ivec2 m_start;
		ivec2 m_finish;
	};
	PathContextCPU(const Description& desc)
	{
		m_desc = desc;
	}


	bool isPath(const ivec2 i)const
	{
		auto wh_m = m_desc.m_size >> 3;
		ivec2 m = i >> 3;
		ivec2 c = i - (m << 3);
		return (m_field[m.x + m.y*wh_m.x] & (1ull << (c.x + c.y * 8))) == 0;
	}

	Description m_desc;
	std::vector<uint64_t> m_field;

};

struct PathContext
{
	struct SparseMap
	{
		uint32_t index;
		uint32_t child_index;
		uint64_t map;
	};
	struct PathNode
	{
		uint32_t cost;
	};

	PathContext(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		auto size = gi2d_context->RenderSize.x*gi2d_context->RenderSize.y / 1;
 		b_sparse_map = context->m_storage_memory.allocateMemory<SparseMap>({ size, {} });
		b_sparse_map_counter = context->m_storage_memory.allocateMemory<uint32_t>({ 1, {} });
		b_sparse_map_hierarchy_counter = context->m_storage_memory.allocateMemory<uvec4>({ 4, {} });
		b_node = context->m_storage_memory.allocateMemory<PathNode>({ size, {} });

		{

			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(std::size(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(std::size(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
					b_sparse_map.getInfo(),
					b_sparse_map_counter.getInfo(),
					b_sparse_map_hierarchy_counter.getInfo(),
					b_node.getInfo(),
				};

				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(std::size(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device->updateDescriptorSets(std::size(write), write, 0, nullptr);
			}

		}

	}

	std::shared_ptr<btr::Context> m_context;

	btr::BufferMemoryEx<SparseMap> b_sparse_map;
	btr::BufferMemoryEx<uint32_t> b_sparse_map_counter;
	btr::BufferMemoryEx<uvec4> b_sparse_map_hierarchy_counter;
	btr::BufferMemoryEx<PathNode> b_node;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;
};

struct Path_Process
{
	Path_Process(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<PathContext>& path_context, const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_context = context;
		m_path_context = path_context;
		m_gi2d_context = gi2d_context;

		{
			const char* name[] =
			{
				"Path_BuildTree.comp.spv",
				"Path_BuildTreeNode.comp.spv",

				"Path_DrawTree.comp.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_path_context->m_descriptor_set_layout.get(),
					m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
				};

				vk::PushConstantRange constants[] = {
					vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setSize(4).setOffset(0),

				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
				pipeline_layout_info.setPPushConstantRanges(constants);
				m_pipeline_layout[PipelineLayout_BuildTree] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					m_path_context->m_descriptor_set_layout.get(),
					m_gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					RenderTarget::s_descriptor_set_layout.get(),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_DrawTree] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			}
		}

		// pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 3> shader_info;
			shader_info[0].setModule(m_shader[Shader_BuildTree].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_BuildTreeNode].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_DrawTree].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_BuildTree].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_BuildTree].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_DrawTree].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[Pipeline_BuildTree] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_BuildTreeNode] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_DrawTree] = std::move(compute_pipeline[2]);
		}

	}

	void executeBuildTree(vk::CommandBuffer cmd)
	{
		vk::DescriptorSet descriptorsets[] = {
			m_path_context->m_descriptor_set.get(),
			m_gi2d_context->getDescriptorSet(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_BuildTree].get(), 0, std::size(descriptorsets), descriptorsets, 0, nullptr);

		{

			vk::BufferMemoryBarrier to_write[] = {
				m_gi2d_context->b_diffuse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

		}
		auto count = m_gi2d_context->m_gi2d_info.getsize(1);
		{

			std::array<uvec4, 4> v = {
				uvec4{count, 1, 1, 0},
				uvec4{0, 1, 1, 0},
				uvec4{0, 1, 1, 0},
				uvec4{0, 1, 1, 0},
			};
			cmd.updateBuffer<std::array<uvec4, 4>>(m_path_context->b_sparse_map_hierarchy_counter.getInfo().buffer, m_path_context->b_sparse_map_hierarchy_counter.getInfo().offset, v);
			cmd.updateBuffer<uint32_t>(m_path_context->b_sparse_map_counter.getInfo().buffer, m_path_context->b_sparse_map_counter.getInfo().offset, count);


			vk::BufferMemoryBarrier barrier[] = {
				m_path_context->b_sparse_map_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				m_path_context->b_sparse_map_hierarchy_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				m_path_context->b_sparse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader|vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
				{}, 0, nullptr, std::size(barrier), barrier, 0, nullptr);

		}

		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_BuildTree].get());

			cmd.pushConstants<uint32_t>(m_pipeline_layout[PipelineLayout_BuildTree].get(), vk::ShaderStageFlagBits::eCompute, 0, { 0 });

			auto num = app::calcDipatchGroups(uvec3(glm::sqrt(count), glm::sqrt(count), 1), uvec3(8, 8, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		for (uint32_t i = 1; i < 2; i++)
		{
			vk::BufferMemoryBarrier barrier[] = {
				m_path_context->b_sparse_map_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				m_path_context->b_sparse_map_hierarchy_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
				m_path_context->b_sparse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect | vk::PipelineStageFlagBits::eComputeShader,
				{}, 0, nullptr, std::size(barrier), barrier, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_BuildTreeNode].get());

			cmd.pushConstants<uint32_t>(m_pipeline_layout[PipelineLayout_BuildTree].get(), vk::ShaderStageFlagBits::eCompute, 0, { i });

			cmd.dispatchIndirect(m_path_context->b_sparse_map_hierarchy_counter.getInfo().buffer, m_path_context->b_sparse_map_hierarchy_counter.getInfo().offset + (i-1) * sizeof(vec4));
		}
	}

	void executeDrawTree(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		vk::BufferMemoryBarrier barrier[] = {
			m_path_context->b_sparse_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader,
			{}, 0, nullptr, std::size(barrier), barrier, 0, nullptr);

		vk::DescriptorSet descriptorsets[] = {
			m_path_context->m_descriptor_set.get(),
			m_gi2d_context->getDescriptorSet(),
			render_target->m_descriptor.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DrawTree].get(), 0, std::size(descriptorsets), descriptorsets, 0, nullptr);

		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_DrawTree].get());

			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderSize.x, m_gi2d_context->RenderSize.y, 1), uvec3(32, 32, 1));
//			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->RenderSize.x*m_gi2d_context->RenderSize.y, 1, 1), uvec3(1024, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
	}


	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<PathContext> m_path_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;

	enum Shader
	{
		Shader_BuildTree,
		Shader_BuildTreeNode,

		Shader_DrawTree,
		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_BuildTree,
		PipelineLayout_DrawTree,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_BuildTree,
		Pipeline_BuildTreeNode,
		Pipeline_DrawTree,
		Pipeline_Num,
	};

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
};

std::vector<uint64_t> pathmake_noise(int sizex, int sizey);
std::vector<char> pathmake_maze_(int sizex, int sizey);
std::vector<uint64_t> pathmake_maze(int sizex, int sizey);

