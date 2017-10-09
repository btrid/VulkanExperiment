#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <999_game/sScene.h>

struct VoxelizeMap : Voxelize
{
	enum SHADER
	{
		SHADER_COMPUTE_VOXELIZE,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_MODEL_VOXELIZE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_MAKE_VOXEL,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_COMPUTE_MAKE_VOXEL,
		PIPELINE_NUM,
	};

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;

	std::vector<vk::UniqueCommandBuffer> m_make_cmd;

	void setup(std::shared_ptr<btr::Context> context, VoxelPipeline const * const parent)
	{
		auto& gpu = context->m_gpu;
		auto& device = gpu.getDevice();

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "MapVoxelize.comp.spv",vk::ShaderStageFlagBits::eCompute },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = std::move(loadShaderUnique(device.getHandle(), path + shader_info[i].name));
				m_stage_info[i].setStage(shader_info[i].stage);
				m_stage_info[i].setModule(m_shader_module[i].get());
				m_stage_info[i].setPName("main");
			}
		}
		// setup pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				parent->getDescriptorSetLayout(VoxelPipeline::DESCRIPTOR_SET_LAYOUT_VOXELIZE),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_MAP),
				sScene::Order().getDescriptorSetLayout(sScene::DESCRIPTOR_SET_LAYOUT_SCENE),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL] = device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// setup compute pipeline
		{
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(m_stage_info[SHADER_COMPUTE_VOXELIZE])
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PIPELINE_COMPUTE_MAKE_VOXEL] = std::move(compute_pipeline[0]);

		}
	}

	void draw(std::shared_ptr<btr::Context>& context, VoxelPipeline const * const parent, vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_MAKE_VOXEL].get());

		std::vector<vk::DescriptorSet> descriptor_sets = {
			parent->getDescriptorSet(VoxelPipeline::DESCRIPTOR_SET_VOXELIZE),
			sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_MAP),
			sScene::Order().getDescriptorSet(sScene::DESCRIPTOR_SET_SCENE),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get(), 0, descriptor_sets, {});
		cmd.dispatch(1, 1, 64);
	}
};
