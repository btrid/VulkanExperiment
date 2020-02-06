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
#include <btrlib/Singleton.h>
#include <btrlib/cWindow.h>
#include <btrlib/cThreadPool.h>
#include <btrlib/cDebug.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/cCamera.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/Geometry.h>

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "imgui.lib")


struct Sky 
{
	enum Shader
	{
		Shader_Sky_CS,
		Shader_SkyWithTexture_CS,
		Shader_SkyMakeTexture_CS,

		Shader_Sky_VS,
		Shader_Sky_FS,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Sky_CS,
		PipelineLayout_Sky,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Sky_CS,
		Pipeline_SkyWithTexture_CS,
		Pipeline_SkyMakeTexture_CS,
		Pipeline_Sky,
		Pipeline_Num,
	};

	std::shared_ptr<btr::Context> m_context;
	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::ImageCreateInfo m_image_info;
	vk::UniqueImage m_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueImageView m_image_write_view;
	vk::UniqueDeviceMemory m_image_memory;
	vk::UniqueSampler m_image_sampler;

	btr::BufferMemoryEx<vec3> v_sphere;
	btr::BufferMemoryEx<uvec3> v_sphere_i;
	vk::UniqueRenderPass m_renderpass;
	vk::UniqueFramebuffer m_framebuffer;
	uint32_t index_num;


	Sky(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
	{

		m_context = context;
		// descriptor layout
		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] =
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageImage, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}
		// descriptor set
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};

				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			}

			vk::ImageCreateInfo image_info;
			image_info.setExtent(vk::Extent3D(256, 32, 256));
			image_info.setArrayLayers(1);
			image_info.setFormat(vk::Format::eR8Unorm);
			image_info.setImageType(vk::ImageType::e3D);
			image_info.setInitialLayout(vk::ImageLayout::eUndefined);
			image_info.setMipLevels(1);
			image_info.setSamples(vk::SampleCountFlagBits::e1);
			image_info.setSharingMode(vk::SharingMode::eExclusive);
			image_info.setTiling(vk::ImageTiling::eOptimal);
			image_info.setUsage(vk::ImageUsageFlagBits::eSampled| vk::ImageUsageFlagBits::eStorage);
			image_info.setFlags(vk::ImageCreateFlagBits::eMutableFormat);

			m_image = context->m_device.createImageUnique(image_info);
			m_image_info = image_info;

			vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			m_image_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
			context->m_device.bindImageMemory(m_image.get(), m_image_memory.get(), 0);

			vk::ImageViewCreateInfo view_info;
			view_info.setFormat(image_info.format);
			view_info.setImage(m_image.get());
			view_info.subresourceRange.setBaseArrayLayer(0);
			view_info.subresourceRange.setLayerCount(1);
			view_info.subresourceRange.setBaseMipLevel(0);
			view_info.subresourceRange.setLevelCount(1);
			view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
			view_info.setViewType(vk::ImageViewType::e3D);
			view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eIdentity).setB(vk::ComponentSwizzle::eIdentity).setA(vk::ComponentSwizzle::eIdentity);
			m_image_view = context->m_device.createImageViewUnique(view_info);

			view_info.setFormat(vk::Format::eR8Uint);
			m_image_write_view = context->m_device.createImageViewUnique(view_info);

			vk::SamplerCreateInfo sampler_info;
			sampler_info.setAddressModeU(vk::SamplerAddressMode::eRepeat);
			sampler_info.setAddressModeV(vk::SamplerAddressMode::eRepeat);
			sampler_info.setAddressModeW(vk::SamplerAddressMode::eRepeat);
//			sampler_info.setAnisotropyEnable(VK_TRUE);
//			sampler_info.setMaxAnisotropy(1.f);
			sampler_info.setMagFilter(vk::Filter::eLinear);
			sampler_info.setMinFilter(vk::Filter::eLinear);
			sampler_info.setMinLod(1.f);
			sampler_info.setMaxLod(1.f);
			sampler_info.setMipLodBias(0.f);
			sampler_info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
			sampler_info.setUnnormalizedCoordinates(false);
			m_image_sampler = context->m_device.createSamplerUnique(sampler_info);


			vk::ImageMemoryBarrier to_make_barrier;
			to_make_barrier.image = m_image.get();
			to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
			to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
			auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

			{
				auto sphere = Geometry::MakeSphere(3);
				v_sphere = context->m_vertex_memory.allocateMemory<vec3>(std::get<0>(sphere).size());
				v_sphere_i = context->m_vertex_memory.allocateMemory<uvec3>(std::get<1>(sphere).size());

				cmd.updateBuffer<vec3>(v_sphere.getInfo().buffer, v_sphere.getInfo().offset, std::get<0>(sphere));
				cmd.updateBuffer<uvec3>(v_sphere_i.getInfo().buffer, v_sphere_i.getInfo().offset, std::get<1>(sphere));

				index_num = std::get<1>(sphere).size();
			}

			vk::DescriptorImageInfo samplers[] = {
				vk::DescriptorImageInfo(m_image_sampler.get(), m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			vk::DescriptorImageInfo images[] = {
				vk::DescriptorImageInfo().setImageView(m_image_write_view.get()).setImageLayout(vk::ImageLayout::eGeneral), 
			};
			

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(array_length(samplers))
				.setPImageInfo(samplers)
				.setDstBinding(0)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageImage)
				.setDescriptorCount(array_length(images))
				.setPImageInfo(images)
				.setDstBinding(1)
				.setDstArrayElement(0)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		// shader
		{
			const char* name[] =
			{
				"Sky.comp.spv",
				"SkyWithTexture.comp.spv",
				"SkyMakeTexture.comp.spv",
				"Sky.vert.spv",
				"Sky.frag.spv",
			};
			static_assert(array_length(name) == Shader_Num, "not equal shader num");

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
			}
		}

		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					RenderTarget::s_descriptor_set_layout.get(),
					m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_pipeline_layout[PipelineLayout_Sky_CS] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					RenderTarget::s_descriptor_set_layout.get(),
					m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(96).setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_pipeline_layout[PipelineLayout_Sky] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}
		}

		// compute pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 3> shader_info;
			shader_info[0].setModule(m_shader[Shader_Sky_CS].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_SkyWithTexture_CS].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_SkyMakeTexture_CS].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky_CS].get()),
			};
			auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_Sky_CS] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_SkyWithTexture_CS] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_SkyMakeTexture_CS] = std::move(compute_pipeline[2]);
		}

		// レンダーパス
		{
			vk::AttachmentReference color_ref[] = {
				vk::AttachmentReference().setLayout(vk::ImageLayout::eColorAttachmentOptimal).setAttachment(0),
			};

			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			vk::AttachmentDescription attach_desc[] =
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
			renderpass_info.setAttachmentCount(array_length(attach_desc));
			renderpass_info.setPAttachments(attach_desc);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_renderpass = context->m_device.createRenderPassUnique(renderpass_info);

			{
				vk::ImageView view[] = {
					render_target->m_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_renderpass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(render_target->m_info.extent.width);
				framebuffer_info.setHeight(render_target->m_info.extent.height);
				framebuffer_info.setLayers(1);

				m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}
		}

		// graphics pipeline
		{
			vk::PipelineShaderStageCreateInfo shader_info[] =
			{
				vk::PipelineShaderStageCreateInfo().setModule(m_shader[Shader_Sky_VS].get()).setPName("main").setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo().setModule(m_shader[Shader_Sky_FS].get()).setPName("main").setStage(vk::ShaderStageFlagBits::eFragment),
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)render_target->m_info.extent.width, (float)render_target->m_info.extent.height, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(render_target->m_info.extent.width, render_target->m_info.extent.height));
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
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);


			vk::PipelineColorBlendAttachmentState blend_state;
			blend_state.setBlendEnable(VK_FALSE);
			blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA);

			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(1);
			blend_info.setPAttachments(&blend_state);

			vk::VertexInputBindingDescription vinput_binding_desc[] = {
				vk::VertexInputBindingDescription(0, sizeof(vec3), vk::VertexInputRate::eVertex),
			};
			vk::VertexInputAttributeDescription vinput_attr_desc[] = {
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vertex_input_info.setVertexBindingDescriptionCount(array_length(vinput_binding_desc));
			vertex_input_info.setPVertexBindingDescriptions(vinput_binding_desc);
			vertex_input_info.setVertexAttributeDescriptionCount(array_length(vinput_attr_desc));
			vertex_input_info.setPVertexAttributeDescriptions(vinput_attr_desc);

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
				.setLayout(m_pipeline_layout[PipelineLayout_Sky].get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
			};
			auto graphics_pipeline = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
			m_pipeline[Pipeline_Sky] = std::move(graphics_pipeline[0]);
		}

	}

	void execute(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);
		{
			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky_CS].get(), 0, array_length(descs), descs, 0, nullptr);

			cmd.pushConstants<float>(m_pipeline_layout[PipelineLayout_Sky_CS].get(), vk::ShaderStageFlagBits::eCompute, 0, sGlobal::Order().getTotalTime());

		}

		// make texture
		_label.insert("make cloud map");
		{
			{

				std::array<vk::ImageMemoryBarrier, 1> image_barrier;
				image_barrier[0].setImage(m_image.get());
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[0].setOldLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eShaderRead);
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}


			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyMakeTexture_CS].get());

			auto num = app::calcDipatchGroups(uvec3(m_image_info.extent.width, m_image_info.extent.height, m_image_info.extent.depth), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// render_targetに書く
		_label.insert("render cloud");
		if(1)
		{
			{

				std::array<vk::ImageMemoryBarrier, 2> image_barrier;
				image_barrier[0].setImage(render_target->m_image);
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setImage(m_image.get());
				image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyWithTexture_CS].get());

			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
		else
		{
			{

				std::array<vk::ImageMemoryBarrier, 2> image_barrier;
				image_barrier[0].setImage(render_target->m_image);
				image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
				image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
				image_barrier[1].setImage(m_image.get());
				image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
				image_barrier[1].setOldLayout(vk::ImageLayout::eGeneral);
				image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eShaderWrite);
				image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eBottomOfPipe|vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eColorAttachmentOutput|vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
			}

			{
				vk::RenderPassBeginInfo begin_render_Info;
				begin_render_Info.setRenderPass(m_renderpass.get());
				begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(render_target->m_info.extent.width, render_target->m_info.extent.height)));
				begin_render_Info.setFramebuffer(m_framebuffer.get());

				begin_render_Info.setClearValueCount(1);
				auto color = vk::ClearValue(vk::ClearColorValue(std::array<uint32_t, 4>{}));
				begin_render_Info.setPClearValues(&color);
				cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

				vk::DescriptorSet descs[] =
				{
					render_target->m_descriptor.get(),
					m_descriptor_set.get(),
				};
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Sky].get(), 0, array_length(descs), descs, 0, nullptr);

				struct S
				{
					mat4 PV;
					vec4 CamPos;
				} s = { glm::perspective(glm::radians(45.f), 1.f, 0.01f, 10000.f) * glm::lookAt(vec3(0.f, 1.f, 0.f), vec3(0.f, 1.f, 10.f), vec3(0.f, -1.f, 0.f)), vec4(0.f, 1.f, 0.f, 0.f) };
				cmd.pushConstants<S>(m_pipeline_layout[PipelineLayout_Sky].get(), vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eFragment, 0, s);

				cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Sky].get());
				cmd.bindVertexBuffers(0, { v_sphere.getInfo().buffer }, { v_sphere.getInfo().offset });
				cmd.bindIndexBuffer(v_sphere_i.getInfo().buffer, v_sphere_i.getInfo().offset, vk::IndexType::eUint32);
				cmd.drawIndexed(index_num, 1, 0, 0, 0);

				cmd.endRenderPass();

			}

		}
	}
};
int main()
{
	btr::setResourceAppPath("..\\..\\resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(0.f, -500.f, 800.f);
	camera->getData().m_target = vec3(0.f, -100.f, 0.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);
	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());
	Sky sky(context, app.m_window->getFrontBuffer());
	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_sky,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_sky");
				sky.execute(cmd, app.m_window->getFrontBuffer());
				cmd.end();
				cmds[cmd_sky] = cmd;
			}

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			app.submit(std::move(cmds));
		}

		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

