#include <btrlib/Define.h>

#include <btrlib/Context.h>

#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cModel.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/sCameraManager.h>
#include <applib/sAppImGui.h>
#include <applib/GraphicsResource.h>
#include <applib/DrawHelper.h>


#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


#include "SNLib/MMSurfaceNet.h"
#include "SNLib/MMGeometryGL.h"

std::vector<uint16_t> makeSpheres(int numSpheres, int demension[3], float voxelSize[3])
{
	int size = demension[0] * demension[1] * demension[2];
	std::vector<float> dists(size);
	std::vector<uint16_t> data(size);

	int maxSize = demension[0] > demension[1] ? demension[0] : demension[1];
	maxSize = (maxSize > demension[2]) ? maxSize : demension[2];
	float maxDist = (float)maxSize;
	float* pDist = dists.data();
	unsigned short* pData = data.data();
	for (int k = 0; k < demension[2]; k++) {
		for (int j = 0; j < demension[1]; j++) {
			for (int i = 0; i < demension[0]; i++) {
				*pData++ = 0;
				*pDist++ = -maxDist;
			}
		}
	}

	// Add numSphere spheres to the data volume. Each sphere has a unique material index and
	// a random center point and radius.
	int largePrime = 7919;
	float invLargePrime = 1.0f / 7919.0f;
	srand(time(NULL));
	float meanRadius = (float(maxSize)) / sqrt(2.0 + float(numSpheres));
	for (int idx = 0; idx < numSpheres; idx++) {
		unsigned short index = (unsigned short)idx + 1;
		float cx = demension[0] * ((rand() % largePrime) * invLargePrime);
		float cy = demension[1] * ((rand() % largePrime) * invLargePrime);
		float cz = demension[2] * ((rand() % largePrime) * invLargePrime);
		float radius = meanRadius * (0.5f + (rand() % largePrime) * invLargePrime);
		int i0 = std::max(0, (int)(cx - radius));
		int i1 = std::min(demension[0] - 1, (int)(cx + radius));
		int j0 = std::max(0, (int)(cy - radius));
		int j1 = std::min(demension[1] - 1, (int)(cy + radius));
		int k0 = std::max(0, (int)(cz - radius));
		int k1 = std::min(demension[2] - 1, (int)(cz + radius));
		for (int k = k0; k <= k1; k++) {
			float dzSqr = (cz - k) * (cz - k);
			for (int j = j0; j <= j1; j++) {
				float dySqr = (cy - j) * (cy - j);
				for (int i = i0; i <= i1; i++) {
					float dxSqr = (cx - i) * (cx - i);
					float distSqr = dxSqr + dySqr + dzSqr;
					if (distSqr < radius * radius) {
						int idx = i + j * demension[0] + k * demension[0] * demension[1];
						float dist = radius - sqrt(distSqr);
						if (dist > dists[idx]) {
							data[idx] = index;
							dists[idx] = dist;
						}
					}
				}
			}
		}
	}
	return data;
}

struct SurfaceNets
{
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS;

	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_PL;

	SurfaceNets(btr::Context& ctx, const std::shared_ptr<RenderTarget>& rt)
	{
// 		// descriptor set layout
// 		{
// 			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
// 			vk::DescriptorSetLayoutBinding binding[] = {
// 				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
// 			};
// 			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
// 			desc_layout_info.setBindingCount(array_length(binding));
// 			desc_layout_info.setPBindings(binding);
// 			m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
// 		}
// 
// 		// descriptor set
// 		{
// 
// 
// 			vk::DescriptorSetLayout layouts[] =
// 			{
// 				m_DSL.get(),
// 			};
// 			vk::DescriptorSetAllocateInfo desc_info;
// 			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
// 			desc_info.setDescriptorSetCount(array_length(layouts));
// 			desc_info.setPSetLayouts(layouts);
// 			m_DS = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);
// 
// 		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
		{
			{"SN_Render.vert.spv", vk::ShaderStageFlagBits::eVertex},
			{"SN_Render.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
		vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt->m_resolution.width, (float)rt->m_resolution.height, 0.f, 1.f);
		vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt->m_resolution.width, rt->m_resolution.height));
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

		auto bindings = {
			vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(MMGeometryGL::GLVertex)),
		};
		auto attributes = {
			vk::VertexInputAttributeDescription().setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(offsetof(MMGeometryGL::GLVertex, pos)),
			vk::VertexInputAttributeDescription().setBinding(0).setLocation(1).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(offsetof(MMGeometryGL::GLVertex, norm)),
			vk::VertexInputAttributeDescription().setBinding(0).setLocation(2).setFormat(vk::Format::eR32G32Sfloat).setOffset(offsetof(MMGeometryGL::GLVertex, tex)),
		};
		vk::PipelineVertexInputStateCreateInfo vertex_input_info;
		vertex_input_info.setVertexBindingDescriptions(bindings);
		vertex_input_info.setVertexAttributeDescriptions(attributes);

		vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
			vk::GraphicsPipelineCreateInfo()
			.setStageCount(shaderStages.size())
			.setPStages(shaderStages.data())
			.setPVertexInputState(&vertex_input_info)
			.setPInputAssemblyState(&assembly_info)
			.setPViewportState(&viewportInfo)
			.setPRasterizationState(&rasterization_info)
			.setPMultisampleState(&sample_info)
			.setLayout(m_PL.get())
			.setPDepthStencilState(&depth_stencil_info)
			.setPColorBlendState(&blend_info);

		auto color_formats = { rt->m_info.format };
		vk::PipelineRenderingCreateInfoKHR pipeline_rendering_create_info = vk::PipelineRenderingCreateInfoKHR().setColorAttachmentFormats(color_formats).setDepthAttachmentFormat(rt->m_depth_info.format);

		graphics_pipeline_info.setPNext(&pipeline_rendering_create_info);
		m_pipeline = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;
	}

	void execute(vk::CommandBuffer cmd, const std::shared_ptr<RenderTarget>& rt, btr::BufferMemoryEx<MMGeometryGL::GLVertex>& v, btr::BufferMemoryEx<uint32_t>& i, uint32_t inum)
	{
		{
			vk::ImageMemoryBarrier image_barrier[1];
			image_barrier[0].setImage(rt->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});

		cmd.bindIndexBuffer(i.getInfo().buffer, i.getInfo().offset, vk::IndexType::eUint32);
		cmd.bindVertexBuffers(0, { v.getInfo().buffer }, {v.getInfo().offset});

		vk::RenderingAttachmentInfoKHR color[] = {
			vk::RenderingAttachmentInfoKHR().setImageView(rt->m_view).setStoreOp(vk::AttachmentStoreOp::eStore).setLoadOp(vk::AttachmentLoadOp::eLoad).setImageLayout(vk::ImageLayout::eColorAttachmentOptimal),
		};
		vk::RenderingAttachmentInfoKHR depth[] = {
			vk::RenderingAttachmentInfoKHR().setImageView(rt->m_depth_view).setStoreOp(vk::AttachmentStoreOp::eStore).setLoadOp(vk::AttachmentLoadOp::eLoad).setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
		};

		vk::RenderingInfoKHR rendering_info;
		rendering_info.colorAttachmentCount = array_size(color);
		rendering_info.pColorAttachments = color;
		rendering_info.pDepthAttachment = depth;
		rendering_info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt->m_info.extent.width, rt->m_info.extent.height)));
		rendering_info.setLayerCount(1);
//		rendering_info.setViewMask(1);

		cmd.beginRenderingKHR(rendering_info);

		cmd.drawIndexed(inum, 1, 0, 0, 0);

		cmd.endRenderingKHR();

	}
};
int main()
{
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(90.0f, 89.0f, -70.0f);
	camera->getData().m_target = vec3(30.f, 30.f, 30.f);
	camera->getData().m_up = vec3(0.f, 1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 50000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);


	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	DrawHelper draw_helper{ context };

	int dimensions[3] = {64, 64, 64};
	float voxelSize[3] = {1.f, 1.f, 1.f};
	int numSpheres = 20;

	// Create the model and its SurfaceNet
	auto modeldata = makeSpheres(numSpheres, dimensions, voxelSize);

	MMSurfaceNet surfaceNet(modeldata.data(), dimensions, voxelSize);
	MMSurfaceNet::RelaxAttrs relaxAttrs;
	relaxAttrs.relaxFactor = 0.5f;
	relaxAttrs.numRelaxIterations = 20;
	relaxAttrs.maxDistFromCellCenter = 1.0f;
	surfaceNet.relax(relaxAttrs);

	MMGeometryGL Geometry(&surfaceNet);
	
	auto vertex = context->m_vertex_memory.allocateMemory<MMGeometryGL::GLVertex>(Geometry.numVertices());
	auto index = context->m_vertex_memory.allocateMemory<uint>(Geometry.numIndices());
	{
		auto staging_vertex = context->m_staging_memory.allocateMemory<MMGeometryGL::GLVertex>(Geometry.numVertices(), true);
		memcpy(staging_vertex.getMappedPtr(), Geometry.vertices(), sizeof(MMGeometryGL::GLVertex) * Geometry.numVertices());
		auto staging_index = context->m_staging_memory.allocateMemory<uint>(Geometry.numIndices(), true);
		memcpy(staging_index.getMappedPtr(), Geometry.indices(), sizeof(uint) * Geometry.numIndices());
		vk::BufferCopy copy[] = {
			vk::BufferCopy().setSize(sizeof(MMGeometryGL::GLVertex) * Geometry.numVertices()).setSrcOffset(staging_vertex.getInfo().offset).setDstOffset(vertex.getInfo().offset),
			vk::BufferCopy().setSize(sizeof(uint) * Geometry.numIndices()).setSrcOffset(staging_index.getInfo().offset).setDstOffset(index.getInfo().offset),
		};
		setup_cmd.copyBuffer(staging_index.getInfo().buffer, vertex.getInfo().buffer, array_size(copy), copy);
	}
	SurfaceNets sn(*context, render_target);

	app.setup();

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
				sn.execute(cmd, render_target, vertex, index, Geometry.numIndices());
				{
					sAppImGui::Order().Render(cmd);
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
