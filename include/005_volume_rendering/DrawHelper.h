#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/loader.h>
#include <btrlib/cCamera.h>
#include <btrlib/BufferMemory.h>
#include <btrlib/sGlobal.h>
#include <btrlib/Singleton.h>
#include <applib/App.h>

struct DrawHelper : public Singleton<DrawHelper>
{
	friend Singleton<DrawHelper>;

	enum SHADER
	{
		SHADER_VERTEX_PRIMITIVE,
		SHADER_FRAGMENT_PRIMITIVE,
		SHADER_NUM,
	};

	enum DescriptorSetLayout
	{
		DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE,
		DESCRIPTOR_SET_LAYOUT_NUM,
	};
	enum DescriptorSet
	{
		DESCRIPTOR_SET_VOLUME_SCENE,
		DESCRIPTOR_SET_NUM,
	};

	enum PipelineLayout
	{
		PIPELINE_LAYOUT_DRAW_VOLUME,
		PIPELINE_LAYOUT_NUM,
	};
	enum Pipeline
	{
		PIPELINE_DRAW_VOLUME,
		PIPELINE_NUM,
	};
	btr::AllocatedMemory m_box_vertex;
	btr::AllocatedMemory m_box_index;
	btr::AllocatedMemory m_sphere_vertex;
	btr::AllocatedMemory m_sphere_index;

	DrawHelper()
	{

	}
	void setup(std::shared_ptr<btr::Loader>& loader)
	{
		// setup shader
		{
			struct
			{
				const char* name;
				vk::ShaderStageFlagBits stage;
			}shader_info[] =
			{
				{ "VolumeRender.vert.spv",vk::ShaderStageFlagBits::eVertex },
				{ "VolumeRender.frag.spv",vk::ShaderStageFlagBits::eFragment },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_info[i].setModule(loadShader(loader->m_device.getHandle(), path + shader_info[i].name));
				m_shader_info[i].setStage(shader_info[i].stage);
				m_shader_info[i].setPName("main");
			}
		}

		// setup descriptor_set_layout
		{
			std::vector<std::vector<vk::DescriptorSetLayoutBinding>> bindings(DESCRIPTOR_SET_LAYOUT_NUM);
			bindings[DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE] =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eFragment)
				.setDescriptorCount(1)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setBinding(1),
			};
			for (size_t i = 0; i < DESCRIPTOR_SET_LAYOUT_NUM; i++)
			{
				vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info = vk::DescriptorSetLayoutCreateInfo()
					.setBindingCount(bindings[i].size())
					.setPBindings(bindings[i].data());
				m_descriptor_set_layout[i] = loader->m_device->createDescriptorSetLayout(descriptor_set_layout_info);
			}

		}
		// setup descriptor_set
		{
			vk::DescriptorSetAllocateInfo alloc_info;
			alloc_info.descriptorPool = loader->m_descriptor_pool;
			alloc_info.descriptorSetCount = m_descriptor_set_layout.size();
			alloc_info.pSetLayouts = m_descriptor_set_layout.data();
			auto descriptor_set = loader->m_device->allocateDescriptorSets(alloc_info);
			std::copy(descriptor_set.begin(), descriptor_set.end(), m_descriptor_set.begin());
			{

				std::vector<vk::DescriptorBufferInfo> uniforms = {
					m_volume_scene_gpu.getBufferInfo(),
				};
				std::vector<vk::DescriptorImageInfo> images = {
					vk::DescriptorImageInfo().setSampler(m_volume_sampler).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal).setImageView(m_volume_image_view),
				};
				std::vector<vk::WriteDescriptorSet> write_desc =
				{
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(images.size())
					.setPImageInfo(images.data())
					.setDstBinding(0)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE]),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(uniforms.size())
					.setPBufferInfo(uniforms.data())
					.setDstBinding(1)
					.setDstSet(m_descriptor_set[DESCRIPTOR_SET_VOLUME_SCENE]),
				};
				loader->m_device->updateDescriptorSets(write_desc, {});
			}

		}

		// setup pipeline_layout
		{
			std::vector<vk::DescriptorSetLayout> layouts = {
				m_descriptor_set_layout[DESCRIPTOR_SET_LAYOUT_VOLUME_SCENE],
				sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(layouts.size());
			pipeline_layout_info.setPSetLayouts(layouts.data());
			m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME] = loader->m_device->createPipelineLayout(pipeline_layout_info);
		}

		// setup pipeline
		vk::Extent3D size;
		size.setWidth(640);
		size.setHeight(480);
		size.setDepth(1);
		// pipeline
		// 		{
		// 			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
		// 			{
		// 				vk::ComputePipelineCreateInfo()
		// 				.setStage(m_shader_info[SHADER_COMPUTE_MAP_UPDATE])
		// 				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_FLOOR]),
		// 			};
		// 			auto pipelines = loader->m_device->createComputePipelines(loader->m_cache, compute_pipeline_info);
		// 			m_pipeline[PIPELINE_COMPUTE_MAP] = pipelines[0];
		// 		}
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f);
			std::vector<vk::Rect2D> scissor = {
				vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height))
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount((uint32_t)scissor.size());
			viewportInfo.setPScissors(scissor.data());

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
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				m_shader_info[SHADER_VERTEX_VOLUME],
				m_shader_info[SHADER_FRAGMENT_VOLUME],
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(2)
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PIPELINE_LAYOUT_DRAW_VOLUME])
				.setRenderPass(loader->m_render_pass)
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto pipelines = loader->m_device->createGraphicsPipelines(loader->m_cache, graphics_pipeline_info);
			std::copy(pipelines.begin(), pipelines.end(), m_pipeline.begin());

		}

	}


};