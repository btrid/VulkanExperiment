#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/loader.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>
#include <999_game/MazeGenerator.h>
#include <999_game/sScene.h>
#include <applib/Geometry.h>
#include <applib/sCameraManager.h>

#include <btrlib/VoxelPipeline.h>

struct MapVoxelize : Voxelize
{
	enum SHADER
	{
		SHADER_COMPUTE_VOXELIZE,
		SHADER_VERTEX_VOXELIZE,
		SHADER_FRAGMENT_VOXELIZE,
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

	vk::UniqueRenderPass m_make_voxel_pass;
	std::vector<vk::UniqueFramebuffer> m_make_voxel_framebuffer;

	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

	std::array<vk::UniquePipeline, PIPELINE_NUM> m_pipeline;
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM> m_pipeline_layout;
	std::array<vk::UniqueDescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_NUM> m_descriptor_set_layout;

	std::vector<vk::UniqueCommandBuffer> m_make_cmd;

	void setup(std::shared_ptr<btr::Loader> loader, VoxelPipeline const * const parent, sScene const * const scene)
	{
		auto& gpu = loader->m_gpu;
		auto& device = gpu.getDevice();

		// renderpass
		{
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(0);
			subpass.setPColorAttachments(nullptr);
			subpass.setPDepthStencilAttachment(nullptr);

			vk::RenderPassCreateInfo renderpass_info = vk::RenderPassCreateInfo()
				.setAttachmentCount(0)
				.setPAttachments(nullptr)
				.setSubpassCount(1)
				.setPSubpasses(&subpass);
			m_make_voxel_pass = loader->m_device->createRenderPassUnique(renderpass_info);

			m_make_voxel_framebuffer.resize(loader->m_window->getSwapchain().getBackbufferNum());
			{
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_make_voxel_pass.get());
				framebuffer_info.setAttachmentCount(0);
				framebuffer_info.setPAttachments(nullptr);
				framebuffer_info.setWidth(parent->getVoxelInfo().u_cell_num.x);
				framebuffer_info.setHeight(parent->getVoxelInfo().u_cell_num.y);
				framebuffer_info.setLayers(1);

				for (size_t i = 0; i < m_make_voxel_framebuffer.size(); i++) {
					m_make_voxel_framebuffer[i] = loader->m_device->createFramebufferUnique(framebuffer_info);
				}
			}
		}

		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "MapVoxelize.comp.spv",vk::ShaderStageFlagBits::eCompute },
				{ "MapVoxelize.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "MapVoxelize.frag.spv",vk::ShaderStageFlagBits::eFragment },
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
			auto compute_pipeline = loader->m_device->createComputePipelinesUnique(loader->m_cache.get(), compute_pipeline_info);
			m_pipeline[PIPELINE_COMPUTE_MAKE_VOXEL] = std::move(compute_pipeline[0]);

		}

	}
	virtual void draw(std::shared_ptr<btr::Executer>& executer, VoxelPipeline const * const parent, vk::CommandBuffer cmd) override
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PIPELINE_COMPUTE_MAKE_VOXEL].get());

		std::vector<vk::DescriptorSet> descriptor_sets = {
			parent->getDescriptorSet(VoxelPipeline::DESCRIPTOR_SET_VOXELIZE),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PIPELINE_LAYOUT_MAKE_VOXEL].get(), 0, descriptor_sets, {});
		cmd.dispatch(1, 1, 32);
	}
};


