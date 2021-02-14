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
#include <applib/sCameraManager.h>

#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct Voxel
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
		Pipeline_MakeVoxelBottom,
		Pipeline_MakeVoxelTop,
		Pipeline_RenderVoxel,
		Pipeline_Debug_RenderVoxel,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_MakeVoxel,
		PipelineLayout_RenderVoxel,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_render_framebuffer;

	struct VoxelInfo
	{
		uvec4 reso;
		uvec4 bottom_brick;
		uvec4 top_brick;
		uvec4 bottom_brick_sqrt;
		uvec4 top_brick_sqrt;
		uvec4 bottom_reso;
		uvec4 top_reso;
		uint material_size;
	};
	struct Material
	{
		uint a;
	};
	btr::BufferMemoryEx<VoxelInfo> u_info;
	btr::BufferMemoryEx<uvec4> b_map_bottom;
	btr::BufferMemoryEx<uvec4> b_map_top;
	btr::BufferMemoryEx<uvec4> b_material_counter;
	btr::BufferMemoryEx<uint> b_material_id;
	btr::BufferMemoryEx<uint> b_material;

	VoxelInfo m_info;

	Voxel(btr::Context& ctx, RenderTarget& rt)
	{
		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

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
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL[DSL_Voxel] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		// descriptor set
		{
			m_info.reso = uvec4(2048,512,2048, 1);
			m_info.bottom_brick = uvec4(4, 8, 4, 1);
			m_info.top_brick = uvec4(4, 8, 4, 1);
			m_info.bottom_brick_sqrt = uvec4(2, 3, 2, 1);
			m_info.top_brick_sqrt = uvec4(2, 3, 2, 1);
			m_info.bottom_reso = m_info.reso / m_info.bottom_brick;
			m_info.top_reso = m_info.bottom_reso / m_info.top_brick;
			m_info.material_size = 20000;

			u_info = ctx.m_uniform_memory.allocateMemory<VoxelInfo>(1);
			b_map_top = ctx.m_storage_memory.allocateMemory<uvec4>(m_info.top_reso.x * m_info.top_reso.y * m_info.top_reso.z, {});
			b_map_bottom = ctx.m_storage_memory.allocateMemory<uvec4>(m_info.bottom_reso.x * m_info.bottom_reso.y * m_info.bottom_reso.z, {});
			b_material_counter = ctx.m_storage_memory.allocateMemory<uvec4>(1);
			b_material_id = ctx.m_storage_memory.allocateMemory<uint>(1);
			b_material = ctx.m_storage_memory.allocateMemory<uint>(m_info.material_size);

			cmd.updateBuffer<VoxelInfo>(u_info.getInfo().buffer, u_info.getInfo().offset, m_info);


			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DSL_Voxel].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[DS_Voxel] = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] =
			{
				u_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] =
			{
				b_map_top.getInfo(),
				b_map_bottom.getInfo(),
				b_material_counter.getInfo(),
				b_material_id.getInfo(),
				b_material.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(m_DS[DS_Voxel].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(1)
				.setDstSet(m_DS[DS_Voxel].get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DSL_Voxel].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_MakeVoxel] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DSL_Voxel].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_RenderVoxel] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Voxel_MakeBottom.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_MakeTop.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_Rendering.comp.spv", vk::ShaderStageFlagBits::eCompute},
			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			vk::ComputePipelineCreateInfo compute_pipeline_info[] =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[0])
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[1])
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[2])
				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
			};
			m_pipeline[Pipeline_MakeVoxelBottom] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_MakeVoxelTop] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
			m_pipeline[Pipeline_RenderVoxel] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
		}

		// graphics pipeline render
		{

			// レンダーパス
			{
				vk::AttachmentReference color_ref[] = {
					vk::AttachmentReference().setLayout(vk::ImageLayout::eColorAttachmentOptimal).setAttachment(0),
				};
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(1);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				// sub pass
				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(array_length(color_ref));
				subpass.setPColorAttachments(color_ref);
				subpass.setPDepthStencilAttachment(&depth_ref);

				vk::AttachmentDescription attach_desc[] =
				{
					// color1
					vk::AttachmentDescription()
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					vk::AttachmentDescription()
					.setFormat(rt.m_depth_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(array_length(attach_desc));
				renderpass_info.setPAttachments(attach_desc);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);

				m_render_pass = ctx.m_device.createRenderPassUnique(renderpass_info);

				{
					vk::ImageView view[] = {
						rt.m_view,
						rt.m_depth_view,
					};
					vk::FramebufferCreateInfo framebuffer_info;
					framebuffer_info.setRenderPass(m_render_pass.get());
					framebuffer_info.setAttachmentCount(array_length(view));
					framebuffer_info.setPAttachments(view);
					framebuffer_info.setWidth(rt.m_info.extent.width);
					framebuffer_info.setHeight(rt.m_info.extent.height);
					framebuffer_info.setLayers(1);

					m_render_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);
				}
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Voxel_Render.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"Voxel_Render.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
			assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)rt.m_resolution.width, (float)rt.m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_resolution.width, rt.m_resolution.height));
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

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL[PipelineLayout_RenderVoxel].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline[Pipeline_Debug_RenderVoxel] = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}
	}

	void execute_MakeVoxel(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		_label.insert("Make Voxel Bottom");
		{
			{
				// 			vk::BufferMemoryBarrier barrier[] =
				// 			{
				// 				base.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				// 				base.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				// 				base.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				// 				boolean.m_TLAS.m_AS_buffer.makeMemoryBarrier(vk::AccessFlagBits::eAccelerationStructureWriteKHR, vk::AccessFlagBits::eShaderRead)
				// 			};
				// 			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelBottom].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

			auto num = m_info.bottom_reso.xyz();
			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("Make Voxel Top");
		{
			{
				vk::BufferMemoryBarrier barrier[] =
				{
					b_map_bottom.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelTop].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

			auto num = m_info.top_reso.xyz();
			cmd.dispatch(num.x, num.y, num.z);

		}
	}
	void execute_RenderVoxel(vk::CommandBuffer cmd, RenderTarget& rt)
	{
		{
			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(rt.m_image);
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader,
				{}, {}, { /*array_size(to_read), to_read*/ }, { image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RenderVoxel].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 1, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 2, { rt.m_descriptor.get() }, {});

		auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(8, 8, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}

	void executeDebug_RenderVoxel(vk::CommandBuffer cmd, RenderTarget& rt)
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

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Debug_RenderVoxel].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_RenderVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_RenderVoxel].get(), 1, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_RenderVoxel].get(), 2, { rt.m_descriptor.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_render_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(rt.m_info.extent.width, rt.m_info.extent.height)));
		begin_render_Info.setFramebuffer(m_render_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.draw(m_info.reso.x* m_info.reso.y* m_info.reso.z, 1, 0, 0);

		cmd.endRenderPass();

	}
};

bool intersection(vec3 aabb_min, vec3 aabb_max, vec3 pos, vec3 inv_dir, float& n, float& f)
{
	// https://tavianator.com/fast-branchless-raybounding-box-intersections/
	vec3 t1 = (aabb_min - pos) * inv_dir;
	vec3 t2 = (aabb_max - pos) * inv_dir;

	vec3 tmin = min(t1, t2);
	vec3 tmax = max(t1, t2);

	n = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
	f = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

	return f > n;
}

void dda()
{
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.42.3443&rep=rep1&type=pdf
	vec3 p = vec3(0, 600, 160);
	vec3 d = normalize(vec3(10, -200, 0));
	vec3 inv_d;
	inv_d.x = abs(d.x) < 0.000001 ? 999999.f : 1. / d.x;
	inv_d.y = abs(d.y) < 0.000001 ? 999999.f : 1. / d.y;
	inv_d.z = abs(d.z) < 0.000001 ? 999999.f : 1. / d.z;

	int axis = abs(d).x > abs(d).y ? 0 : 1;
	axis = abs(d)[axis] > abs(d).z ? axis : 2;
	d *= abs(inv_d)[axis];
	inv_d /= abs(inv_d)[axis];

#if 0
	float n, f;
	intersection(vec3(0) + 0.01f, vec3(512) - 0.01f, p, inv_d, n, f);
	p += glm::max(n, 0.f) * d;
	vec3 error = fract(p);
	ivec3 ipos = ivec3(p);
	while (all(lessThan(ipos, ivec3(512))) && all(greaterThanEqual(ipos, ivec3(0))))
	{
		printf("%3d,%3d,%3d\n", ipos.x, ipos.y, ipos.z);
//		error += abs(d);
//		p += ivec3(sign(d) * error);
// 		error = fract(error);

		ivec3 sp = ipos / 4;
 		intersection(vec3(sp)*4.f-0.01f, vec3(sp)*4.f+4.01f, vec3(ipos)+error, inv_d, n, f);
 		error += abs(d) * glm::max(f, 0.f);
 		ipos += ivec3(sign(d) * error);
 		error = fract(error);

	}
#elif 1
	float n, f;
	intersection(vec3(0), vec3(512), p, inv_d, n, f);
	ivec3 ipos = ivec3(p + glm::max(n, 0.f) * d);
	ivec3 igoal = ivec3(p + glm::max(f, 0.f) * d);
	ivec3 idir = igoal - ipos;
	ivec3 isign = sign(idir);
		
	int dominant = abs(idir.x) > abs(idir.y) ? 0 : 1;
	dominant = abs(idir[dominant]) > abs(idir.z) ? dominant : 2;

	ivec2 delta;
	delta.x = 2*abs(idir[(dominant + 1) % 3]) - abs(idir[dominant]);
	delta.y = 2*abs(idir[(dominant + 2) % 3]) - abs(idir[dominant]);

	for (; ipos[dominant] != igoal[dominant];)
	{
		printf("%3d,%3d,%3d\n", ipos.x, ipos.y, ipos.z);
		if (delta.x >= 0)
		{
			ipos[(dominant+1)%3] += isign[(dominant + 1) % 3];
			delta.x -= abs(idir[dominant])*2;
		}
		else if (delta.y >= 0)
		{
			ipos[(dominant + 2) % 3] += isign[(dominant + 2) % 3];
			delta.y -= abs(idir[dominant])*2;
		}
		else
		{
			ipos[dominant] += isign[dominant];
			delta.x += 2 * abs(idir[(dominant + 1) % 3]);
			delta.y += 2 * abs(idir[(dominant + 2) % 3]);
		}

	}
	assert(all(equal(ipos, igoal)));

#else
	// 1voxel版
	vec3 error = mix(fract(p), vec3(1.) - fract(p), step(d, vec3(0.))) * abs(inv_d);
	while (all(lessThan(p, vec3(512.f))) && all(greaterThanEqual(p, vec3(0.f))))
	{
		printf("%6.2f,%6.2f,%6.2f\n", p.x, p.y, p.z);
		int axis = (error.x < error.y) ? 0 : 1;
		axis = error[axis] < error.z ? axis : 2;
		p[axis] += glm::sign(d[axis]);
		error[axis] += abs(inv_d[axis]);
	}
#endif
	int a = 0;
}
int main()
{
	dda();
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-400.f, 1500.f, -1430.f);
	camera->getData().m_target = vec3(800.f, 0.f, 1000.f);
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


	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	Voxel voxel(*context, *app.m_window->getFrontBuffer());
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		voxel.execute_MakeVoxel(cmd);
	}
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
				{
//					voxel.execute_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
					voxel.executeDebug_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
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

