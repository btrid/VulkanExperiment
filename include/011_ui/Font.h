#pragma once

#include <string>
#include <ft2build.h>
#include <freetype/freetype.h>

#include <btrlib/Define.h>
#include <btrlib/Singleton.h>
struct Font
{
};

struct sFont : SingletonEx <sFont>
{
	FT_Library m_library;

	enum Shader
	{
		SHADER_VERT_RENDER,
		SHADER_FRAG_RENDER,

		SHADER_NUM,
	};
	std::array<vk::UniqueShaderModule, SHADER_NUM> m_shader_module;
	vk::ShaderModule getShaderModle(Shader type) { return m_shader_module[type].get(); }


	enum PipelineLayout
	{
		PIPELINE_LAYOUT_RENDER,
		PIPELINE_LAYOUT_NUM,
	};
	std::array<vk::UniquePipelineLayout, PIPELINE_LAYOUT_NUM>	m_pipeline_layout;
	vk::PipelineLayout getPipelineLayout(PipelineLayout type) { return m_pipeline_layout[type].get(); }

	vk::UniqueDescriptorSetLayout	m_descriptor_set_layout;
	vk::UniqueDescriptorSet			m_descriptor_set;

	sFont(const std::shared_ptr<btr::Context>& context)
	{
		// setup shader
		{
			struct
			{
				const char* name;
			}shader_info[] =
			{
				{ "FontRender.vert.spv", },
				{ "FontRender.frag.spv", },
			};
			static_assert(array_length(shader_info) == SHADER_NUM, "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < SHADER_NUM; i++) {
				m_shader_module[i] = loadShaderUnique(context->m_device.getHandle(), path + shader_info[i].name);
			}
		}

		// descriptor
		{
			auto stage = vk::ShaderStageFlagBits::eFragment;
			std::vector<vk::DescriptorSetLayoutBinding> binding =
			{
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(1)
				.setBinding(0),
			};
			m_descriptor_set_layout = btr::Descriptor::createDescriptorSetLayout(context, binding);
			m_descriptor_set = btr::Descriptor::allocateDescriptorSet(context, context->m_descriptor_pool.get(), m_descriptor_set_layout.get());

		}

		// setup pipeline_layout
		{
// 			vk::DescriptorSetLayout layouts[] = {
// //				m_descriptor_set_layout.get(),
// //				sSystem::Order().getSystemDescriptorLayout(),
// 			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
// 			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
// 			pipeline_layout_info.setPSetLayouts(layouts);
			// 		pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
			// 		pipeline_layout_info.setPPushConstantRanges(constants);
			m_pipeline_layout[PIPELINE_LAYOUT_RENDER] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		FT_Init_FreeType(&m_library);

	}

	~sFont()
	{
		FT_Done_FreeType(m_library);
	}

};

struct CmdList
{
	void pushCmd(std::function<void()>&& cmd)
	{
		std::lock_guard<std::mutex> lg(m_cmd_mutex);
		m_cmd_list[sGlobal::Order().getWorkerIndex()].emplace_back(std::move(cmd));
	}
	std::vector<std::function<void()>>& getCmd()
	{
		return std::move(m_cmd_list[sGlobal::Order().getRenderIndex()]);
	}
	~CmdList()
	{
		assert(m_cmd_list[0].empty());
		assert(m_cmd_list[1].empty());
	}

	std::mutex m_cmd_mutex;
	std::array<std::vector<std::function<void()>>, 2> m_cmd_list;

};

struct PipelineDescription
{
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<RenderPassModule> m_render_pass;
};
struct FontRenderPipeline
{
	FontRenderPipeline(const std::shared_ptr<btr::Context>& context, const PipelineDescription& desc)
	{
		m_pipeline_description = desc;
		// setup pipeline
		{
			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eLineStrip),
			};

			auto& render_pass = m_pipeline_description.m_render_pass;
			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_pass->getResolution().width, (float)render_pass->getResolution().height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(render_pass->getResolution().width, render_pass->getResolution().height));
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
			depth_stencil_info.setDepthTestEnable(VK_FALSE);

			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_TRUE)
				.setSrcAlphaBlendFactor(vk::BlendFactor::eOne)
				.setDstAlphaBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setAlphaBlendOp(vk::BlendOp::eAdd)
				.setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha)
				.setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha)
				.setColorBlendOp(vk::BlendOp::eAdd)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::VertexInputAttributeDescription attr[] =
			{
				vk::VertexInputAttributeDescription().setLocation(0).setOffset(0).setBinding(0).setFormat(vk::Format::eR32G32Sint),
			};
			vk::VertexInputBindingDescription binding[] =
			{
				vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(8)
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexAttributeDescriptionCount(array_length(attr));
			vertex_input_info.setPVertexAttributeDescriptions(attr);
			vertex_input_info.setVertexBindingDescriptionCount(array_length(binding));
			vertex_input_info.setPVertexBindingDescriptions(binding);

			vk::PipelineShaderStageCreateInfo shader_info[] = {
				vk::PipelineShaderStageCreateInfo()
				.setModule(sFont::Order().getShaderModle(sFont::SHADER_VERT_RENDER))
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(sFont::Order().getShaderModle(sFont::SHADER_FRAG_RENDER))
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER))
				.setRenderPass(render_pass->getRenderPass())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
//				.setPDynamicState(&dynamic_info),
			};
			auto pipelines = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline = std::move(pipelines[0]);

		}

		FT_New_Face(sFont::Order().m_library, (btr::getResourceAppPath() + "font\\" + "Inconsolata.otf").c_str(), 0, &m_face);
		FT_Load_Glyph(m_face, 0, FT_LOAD_NO_SCALE | FT_LOAD_NO_AUTOHINT | FT_LOAD_NO_BITMAP);
		FT_Set_Char_Size(m_face, 16, 16, 640, 480);
		FT_Set_Pixel_Sizes(m_face, 16, 16);
	}
	~FontRenderPipeline()
	{
		FT_Done_Face(m_face);

	}


	void async(vk::CommandBuffer& cmd)
	{
		auto& outline = m_face->glyph->outline;

		auto index = FT_Get_Char_Index(m_face, 'a');
		FT_Load_Glyph(m_face, index, FT_LOAD_DEFAULT);
		vk::RenderPassBeginInfo begin_render_info;
		begin_render_info.setFramebuffer(m_pipeline_description.m_render_pass->getFramebuffer(m_pipeline_description.m_context->m_window->getSwapchain().m_backbuffer_index));
		begin_render_info.setRenderPass(m_pipeline_description.m_render_pass->getRenderPass());
		begin_render_info.setRenderArea(vk::Rect2D({}, m_pipeline_description.m_render_pass->getResolution()));
		cmd.beginRenderPass(begin_render_info, vk::SubpassContents::eInline);
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
// 		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER), 0, { m_descriptor_set.get() }, {});
// 		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, sFont::Order().getPipelineLayout(sFont::PIPELINE_LAYOUT_RENDER), 1, { sSystem::Order().getSystemDescriptorSet() }, { i * sSystem::Order().getSystemDescriptorStride() });

		btr::BufferMemoryDescriptorEx<ImDrawVert> v_desc;
		v_desc.element_num = outline.n_points;
		v_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto vertex = m_pipeline_description.m_context->m_staging_memory.allocateMemory(v_desc);
		memcpy(vertex.getMappedPtr(), outline.points, sizeof(FT_Vector)*outline.n_points);
		cmd.bindVertexBuffers(0, { vertex.getInfo().buffer }, { vertex.getInfo().offset });

		int voffset = 0;
		for (uint32_t i = 0; i < outline.n_contours; i++)
		{
			cmd.draw(outline.contours[i]-voffset, 1, voffset, 0);
			voffset = outline.contours[i]+1;
		}
		cmd.endRenderPass();
	}

	vk::UniquePipeline m_pipeline;
	CmdList m_cmd_list;
	PipelineDescription m_pipeline_description;

	FT_Face m_face;
};

