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

struct Voxel2
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
		Pipeline_MakeVoxelLeaf,
		Pipeline_MakeVoxelTop,
		Pipeline_MakeVoxelTopChild,
		Pipeline_MakeVoxelMid,

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
		uvec4 reso_brick[2];
	};
	struct Material
	{
		uint a;
	};

	struct InteriorNode
	{
		uvec2 bitmask;
		uint child;
	};
	struct LeafNode
	{
		uint16_t normal;
		uint albedo;
	};
	struct LeafData
	{
		uvec2 bitmask;
		u16vec4 pos_index;
	};

	btr::BufferMemoryEx<VoxelInfo> u_info;
	btr::BufferMemoryEx<int> b_hashmap;
	btr::BufferMemoryEx<uvec4> b_interior_counter;
	btr::BufferMemoryEx<uint> b_leaf_counter;
	btr::BufferMemoryEx<InteriorNode> b_interior;
	btr::BufferMemoryEx<LeafNode> b_leaf;
	btr::BufferMemoryEx<uint> b_leaf_data_counter;
	btr::BufferMemoryEx<LeafData> b_leaf_data;

	VoxelInfo m_info;

	Voxel2(btr::Context& ctx, RenderTarget& rt)
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
				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL[DSL_Voxel] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		// descriptor set
		{
			m_info.reso = uvec4(2048, 512, 2048, 1);
			uint num = m_info.reso.x* m_info.reso.y* m_info.reso.z;
			b_hashmap = ctx.m_storage_memory.allocateMemory<int>(num / 64 / 64);
			u_info = ctx.m_uniform_memory.allocateMemory<VoxelInfo>(1);
			b_interior_counter = ctx.m_storage_memory.allocateMemory<uvec4>(2);
			b_leaf_counter = ctx.m_storage_memory.allocateMemory<uint>(1);
			b_interior = ctx.m_storage_memory.allocateMemory<InteriorNode>(num/64/32);
			b_leaf = ctx.m_storage_memory.allocateMemory<LeafNode>(num/64/32);
			b_leaf_data_counter = ctx.m_storage_memory.allocateMemory<uint>(1);
			b_leaf_data = ctx.m_storage_memory.allocateMemory<LeafData>(num / 64/32);
				
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
				b_hashmap.getInfo(),
				b_interior_counter.getInfo(),
				b_leaf_counter.getInfo(),
				b_interior.getInfo(),
				b_leaf.getInfo(),
				b_leaf_data_counter.getInfo(),
				b_leaf_data.getInfo(),
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
				{"Voxel_Allocate.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_AllocateTop.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_AllocateTopChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_AllocateMid.comp.spv", vk::ShaderStageFlagBits::eCompute},
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
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[3])
				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[4])
				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
			};
			m_pipeline[Pipeline_MakeVoxelLeaf] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_MakeVoxelTop] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
			m_pipeline[Pipeline_MakeVoxelTopChild] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
			m_pipeline[Pipeline_MakeVoxelMid] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
			m_pipeline[Pipeline_RenderVoxel] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[4]).value;
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
				{"Voxel_Render.geom.spv", vk::ShaderStageFlagBits::eGeometry},
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

		{

			{
				vk::BufferMemoryBarrier write[] =
				{
					b_hashmap.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					b_leaf_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					b_leaf_data_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_size(write), write }, {});
			}

			{
				cmd.fillBuffer(b_hashmap.getInfo().buffer, b_hashmap.getInfo().offset, b_hashmap.getInfo().range, -1);
				cmd.updateBuffer<ivec4>(b_interior_counter.getInfo().buffer, b_interior_counter.getInfo().offset, { ivec4{0,1,1,0}, ivec4{0,1,1,0} });
				cmd.fillBuffer(b_leaf_counter.getInfo().buffer, b_leaf_counter.getInfo().offset, b_leaf_counter.getInfo().range, 0);
				cmd.updateBuffer<ivec4>(b_leaf_data_counter.getInfo().buffer, b_leaf_data_counter.getInfo().offset, ivec4(0,1,1,0));
			}

			{
				vk::BufferMemoryBarrier read[] =
				{
					b_hashmap.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_leaf_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
					b_leaf_data_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(read), read }, {});
			}
		}

		_label.insert("Make Voxel");
		{
			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelLeaf].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

 			auto num = m_info.reso.xyz()>>uvec3(2);
 			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("Make Voxel Top");
		{
 			{
 				vk::BufferMemoryBarrier barrier[] =
 				{
					b_interior_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
 				};
 				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect, {}, {}, { array_size(barrier), barrier }, {});
 			}
 
 			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelTop].get());
 			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
 
 			cmd.dispatchIndirect(b_interior_counter.getInfo().buffer, b_interior_counter.getInfo().offset);

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

		cmd.draw(m_info.reso.x * m_info.reso.y * m_info.reso.z, 1, 0, 0);

		cmd.endRenderPass();

	}
};

#include <017_Raytracing/voxel.h>


int main()
{
	voxel_test();
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

	Voxel2 voxel(*context, *app.m_window->getFrontBuffer());
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
					static int t = 0;
					if (t>=0)
					{
						voxel.execute_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
					}
					else
					{
//						voxel.executeDebug_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
					}
//					t++;
//					if (t >= 100)
//					{
//						t -= 103;
//					}
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

