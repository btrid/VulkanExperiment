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

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <btrlib/Context.h>
#include <applib/sImGuiRenderer.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
//#pragma comment(lib, "FreeImage.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

// order-independent-transparency
struct OITPipeline 
{
	virtual void execute(vk::CommandBuffer cmd) = 0;
};

struct OITRenderer
{

	enum
	{
// 		RenderWidth = 1024,
// 		RenderHeight = 1024,
//		RenderDepth = 1,
//		FragmentBufferSize = RenderWidth * RenderHeight*RenderDepth,
	};
	int RenderWidth;
	int RenderHeight;
	int RenderDepth;
	int FragmentBufferSize;

	enum Shader
	{
		//		ShaderClear,
		ShaderMakeFragmentHierarchy,
		ShaderPhotonMapping,
		ShaderRenderingVS,
		ShaderRenderingFS,
		ShaderNum,
	};
	enum PipelineLayout
	{
		PipelineLayoutMakeFragmentHierarchy,
		PipelineLayoutLightCulling,
		PipelineLayoutPhotonMapping,
		PipelineLayoutRendering,
		PipelineLayoutNum,
	};
	enum Pipeline
	{
		PipelineMakeFragmentHierarchy,
		PipelineLightCulling,
		PipelinePhotonMapping,
		PipelineRendering,
		PipelineNum,
	};
	struct Info
	{
		mat4 m_camera_PV;
		uvec4 m_resolution;
		vec4 m_position;
		uint m_emissive_tile_map_max;
	};
	struct Fragment
	{
		vec3 albedo;
	};


	OITRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = context;
		m_render_target = render_target;

		RenderWidth = m_render_target->m_resolution.width;
		RenderHeight = m_render_target->m_resolution.height;
		RenderDepth = 1;
		FragmentBufferSize = RenderWidth * RenderHeight*RenderDepth;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			btr::BufferMemoryDescriptorEx<Info> desc;
			desc.element_num = 1;
			m_fragment_info = context->m_uniform_memory.allocateMemory(desc);

			Info info;
			info.m_position = vec4(0.f, 0.f, 0.f, 0.f);
			info.m_resolution = vec4(RenderWidth, RenderHeight, RenderDepth, 0.f);
			info.m_camera_PV = glm::ortho(-RenderWidth * 0.5f, RenderWidth*0.5f, -RenderHeight * 0.5f, RenderHeight*0.5f);
			info.m_camera_PV *= glm::lookAt(vec3(0., -1.f, 0.f) + info.m_position.xyz(), info.m_position.xyz(), vec3(0.f, 0.f, 1.f));
			info.m_emissive_tile_map_max = 4;
			cmd.updateBuffer<Info>(m_fragment_info.getInfo().buffer, m_fragment_info.getInfo().offset, info);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = 1;
			m_fragment_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<Fragment> desc;
			desc.element_num = FragmentBufferSize;
			m_fragment_buffer = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = FragmentBufferSize;
			m_fragment_map = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int64_t> desc;
			desc.element_num = RenderWidth * RenderHeight / 8 / 8;
			m_fragment_hierarchy = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<ivec3> desc;
			desc.element_num = 1;
			m_emissive_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<vec3> desc;
			desc.element_num = FragmentBufferSize;
			m_emissive_buffer = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = FragmentBufferSize;
			m_emissive_map = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = FragmentBufferSize;
			m_emissive_tile_counter = context->m_storage_memory.allocateMemory(desc);
		}
		{
			btr::BufferMemoryDescriptorEx<int32_t> desc;
			desc.element_num = FragmentBufferSize;
			m_emissive_tile_map = context->m_storage_memory.allocateMemory(desc);
		}

		{
			btr::BufferMemoryDescriptorEx<vec4> desc;
			desc.element_num = FragmentBufferSize;
			m_color = context->m_storage_memory.allocateMemory(desc);

		}
		{
			auto stage = vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(1)
				.setBinding(0),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(1),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(2),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(3),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(4),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(10),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(11),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(12),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(13),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(14),
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(1)
				.setBinding(20),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout = context->m_device->createDescriptorSetLayoutUnique(desc_layout_info);

		}
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descriptor_set = std::move(context->m_device->allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] = {
				m_fragment_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				m_fragment_counter.getInfo(),
				m_fragment_buffer.getInfo(),
				m_fragment_map.getInfo(),
				m_fragment_hierarchy.getInfo(),
			};
			vk::DescriptorBufferInfo emissive_storages[] = {
				m_emissive_counter.getInfo(),
				m_emissive_buffer.getInfo(),
				m_emissive_map.getInfo(),
				m_emissive_tile_counter.getInfo(),
				m_emissive_tile_map.getInfo(),
			};
			vk::DescriptorBufferInfo output_storages[] = {
				m_color.getInfo(),
			};
			vk::WriteDescriptorSet write[] = {
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(1)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(emissive_storages))
				.setPBufferInfo(emissive_storages)
				.setDstBinding(10)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(output_storages))
				.setPBufferInfo(output_storages)
				.setDstBinding(20)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}


		{
			const char* name[] =
			{
				"MakeFragmentHierarchy.comp.spv",
				"PhotonMapping.comp.spv",
				"PMRendering.vert.spv",
				"PMRendering.frag.spv",
			};
			static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}

		}

		// pipeline layout
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get()
			};
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PipelineLayoutPhotonMapping] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
			m_pipeline_layout[PipelineLayoutRendering] = context->m_device->createPipelineLayoutUnique(pipeline_layout_info);
		}

		// pipeline
		{
			vk::PipelineShaderStageCreateInfo shader_info[2];
			shader_info[0].setModule(m_shader[ShaderMakeFragmentHierarchy].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[ShaderPhotonMapping].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");

			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayoutPhotonMapping].get()),
			};
			auto compute_pipeline = context->m_device->createComputePipelinesUnique(context->m_cache.get(), compute_pipeline_info);
			m_pipeline[PipelineMakeFragmentHierarchy] = std::move(compute_pipeline[0]);
			m_pipeline[PipelinePhotonMapping] = std::move(compute_pipeline[1]);
		}

		// レンダーパス
		{
			vk::AttachmentReference color_ref[] = {
				vk::AttachmentReference()
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setAttachment(0)
			};
			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(render_target->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}
		{
			vk::ImageView view[] = {
				render_target->m_view,
			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(render_target->m_info.extent.width);
			framebuffer_info.setHeight(render_target->m_info.extent.height);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
		}

		{
			vk::PipelineShaderStageCreateInfo shader_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[ShaderRenderingVS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[ShaderRenderingFS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleStrip);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_target->m_resolution.width, (float)render_target->m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), render_target->m_resolution);
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
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
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
			blend_info.setAttachmentCount((uint32_t)blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayoutRendering].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto graphics_pipeline = context->m_device->createGraphicsPipelinesUnique(context->m_cache.get(), graphics_pipeline_info);
			m_pipeline[PipelineRendering] = std::move(graphics_pipeline[0]);

		}
	}

	vk::CommandBuffer execute(const std::vector<OITPipeline*>& pipeline)
	{
		auto cmd = m_context->m_cmd_pool->allocCmdOnetime(0);
		// pre
		{
			vk::BufferMemoryBarrier to_clear = m_color.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { to_clear }, {});

			// clear
			union {
				uint32_t u;
				float f;
			}u2f;
			u2f.f = 0.f;
			cmd.fillBuffer(m_color.getInfo().buffer, m_color.getInfo().offset, m_color.getInfo().range, u2f.u);
			cmd.fillBuffer(m_fragment_counter.getInfo().buffer, m_fragment_counter.getInfo().offset, m_fragment_counter.getInfo().range, 0u);
			cmd.fillBuffer(m_fragment_buffer.getInfo().buffer, m_fragment_buffer.getInfo().offset, m_fragment_buffer.getInfo().range, 0u);
			cmd.fillBuffer(m_fragment_map.getInfo().buffer, m_fragment_map.getInfo().offset, m_fragment_map.getInfo().range, 0u);
			cmd.updateBuffer<ivec3>(m_emissive_counter.getInfo().buffer, m_emissive_counter.getInfo().offset, ivec3(0, 1, 1));
			cmd.fillBuffer(m_emissive_buffer.getInfo().buffer, m_emissive_buffer.getInfo().offset, m_emissive_buffer.getInfo().range, 0u);
			cmd.fillBuffer(m_emissive_map.getInfo().buffer, m_emissive_map.getInfo().offset, m_emissive_map.getInfo().range, 0u);
		}
		// exe
		for (auto& p : pipeline)
		{
			p->execute(cmd);
		}

		// post
		{
			auto to_write = m_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_write }, {});

			// make hierarchy
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelineMakeFragmentHierarchy].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutMakeFragmentHierarchy].get(), 0, m_descriptor_set.get(), {});
			cmd.dispatch(RenderWidth / 8, RenderHeight / 8, 1);
		}

		{
			auto to_read = m_fragment_hierarchy.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { to_read }, {});

			// photonmapping
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[PipelinePhotonMapping].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayoutPhotonMapping].get(), 0, m_descriptor_set.get(), {});
			cmd.dispatch(20, 20, 1);
		}

		{
			vk::BufferMemoryBarrier to_render = m_color.makeMemoryBarrier(vk::AccessFlagBits::eMemoryWrite, vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, { to_render }, {});

			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_render_pass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
			begin_render_Info.setFramebuffer(m_framebuffer.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			// render_targetに書く
			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[PipelineRendering].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayoutRendering].get(), 0, m_descriptor_set.get(), {});
			cmd.draw(3, 1, 0, 0);

			cmd.endRenderPass();
		}

		cmd.end();
		return cmd;
	}
	btr::BufferMemoryEx<Info> m_fragment_info;
	btr::BufferMemoryEx<int32_t> m_fragment_counter;
	btr::BufferMemoryEx<Fragment> m_fragment_buffer;
	btr::BufferMemoryEx<int32_t> m_fragment_map;
	btr::BufferMemoryEx<int64_t> m_fragment_hierarchy;
	btr::BufferMemoryEx<ivec3> m_emissive_counter;
	btr::BufferMemoryEx<vec3> m_emissive_buffer;
	btr::BufferMemoryEx<int32_t> m_emissive_map;
	btr::BufferMemoryEx<int32_t> m_emissive_tile_counter;
	btr::BufferMemoryEx<int32_t> m_emissive_tile_map;
	btr::BufferMemoryEx<vec4> m_color;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	std::shared_ptr<RenderTarget> m_render_target;
	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

	vk::UniqueShaderModule m_shader[ShaderNum];
	std::array<vk::UniquePipelineLayout, PipelineLayoutNum> m_pipeline_layout;
	std::array<vk::UniquePipeline, PipelineNum> m_pipeline;

	std::shared_ptr<btr::Context> m_context;
};

struct AppModelOIT : public OITPipeline
{
	AppModelOIT(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<OITRenderer>& renderer)
	{
		{
			const char* name[] =
			{
				"OITAppModel.vert.spv",
				"OITAppModel.frag.spv",
			};
			static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

			std::string path = btr::getResourceAppPath() + "shader\\binary\\";
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}

		}
	}
	void execute(vk::CommandBuffer cmd)override
	{

	}
	vk::UniqueShaderModule m_shader[2];
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;
};

struct DebugOIT : public OITPipeline
{
	DebugOIT(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<OITRenderer>& renderer)
	{
		m_context = context;
		m_renderer = renderer;
		std::vector<OITRenderer::Fragment> map_data(renderer->RenderWidth*renderer->RenderHeight);
		for (auto& m : map_data)
		{
			m.albedo = vec3(0.);
			if (std::rand() % 100 == 0)
			{
				m.albedo = vec3(1.f);
			}
		}

		btr::BufferMemoryDescriptorEx<OITRenderer::Fragment> desc;
		desc.element_num = map_data.size();
		m_map_data = m_context->m_storage_memory.allocateMemory(desc);

		desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging = m_context->m_staging_memory.allocateMemory(desc);
		memcpy_s(staging.getMappedPtr(), vector_sizeof(map_data), map_data.data(), vector_sizeof(map_data));

		vk::BufferCopy copy;
		copy.setSrcOffset(staging.getInfo().offset);
		copy.setDstOffset(m_map_data.getInfo().offset);
		copy.setSize(m_map_data.getInfo().range);

		auto cmd = m_context->m_cmd_pool->allocCmdTempolary(0);
		cmd.copyBuffer(staging.getInfo().buffer, m_map_data.getInfo().buffer, copy);
	}
	void execute(vk::CommandBuffer cmd)override
	{
		vk::BufferMemoryBarrier to_write[] =
		{
			m_renderer->m_emissive_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferWrite),
			m_renderer->m_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

		cmd.updateBuffer<ivec3>(m_renderer->m_emissive_counter.getInfo().buffer, m_renderer->m_emissive_counter.getInfo().offset, ivec3(1, 1, 1));

		auto e_buffer = m_renderer->m_emissive_buffer.getInfo();
		cmd.updateBuffer<vec3>(e_buffer.buffer, e_buffer.offset, vec3(100.f, 100.f, 0.f));

//		cmd.updateBuffer(m_renderer->m_fragment_buffer.getInfo().buffer, m_renderer->m_fragment_buffer.getInfo().offset, vector_sizeof(m_map_data), m_map_data.data());
		vk::BufferCopy copy;
		copy.setSrcOffset(m_map_data.getInfo().offset);
		copy.setDstOffset(m_renderer->m_fragment_buffer.getInfo().offset);
		copy.setSize(m_map_data.getInfo().range);

		cmd.copyBuffer(m_map_data.getInfo().buffer, m_renderer->m_fragment_buffer.getInfo().buffer, copy);

		vk::BufferMemoryBarrier to_read[] = 
		{
			m_renderer->m_emissive_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
			m_renderer->m_fragment_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eIndirectCommandRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<OITRenderer> m_renderer;
	btr::BufferMemoryEx<OITRenderer::Fragment> m_map_data;
};

int main()
{
	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 640;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(640, 640);
	app::App app(app_desc);

	auto context = app.m_context;
	ClearPipeline clear_pipeline(context, app.m_window->getRenderTarget());
	PresentPipeline present_pipeline(context, app.m_window->getRenderTarget(), app.m_window->getSwapchainPtr());

	std::shared_ptr<OITRenderer> oit_renderer = std::make_shared<OITRenderer>(context, app.m_window->getRenderTarget());
	DebugOIT oit_debug(context, oit_renderer);
//	AppModelOIT model_oit(context, oit_renderer);
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum 
			{
				cmd_clear,
				cmd_pm,
				cmd_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			cmds[cmd_clear] = clear_pipeline.execute();
			auto p = std::vector<OITPipeline*>{&oit_debug};
			cmds[cmd_pm] = oit_renderer->execute(p);
			cmds[cmd_present] = present_pipeline.execute();
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

