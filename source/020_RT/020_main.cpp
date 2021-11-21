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


#include <tinygltf/tiny_gltf.h>
#include <gli/gli/gli.hpp>

#include <020_RT/Resource.h>

#define to_str(_a) #_a

struct BRDFLookupTable
{
	AppImage m_brgf_lut;

	BRDFLookupTable(btr::Context& ctx, vk::CommandBuffer cmd)
	{
		const vk::Format format = vk::Format::eR16G16Sfloat;
		const int32_t dim = 512;

		// Image
		vk::ImageCreateInfo image_info;
		image_info.imageType = vk::ImageType::e2D;
		image_info.format = format;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = vk::SampleCountFlagBits::e1;
		image_info.tiling = vk::ImageTiling::eOptimal;
		image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
		image_info.sharingMode = vk::SharingMode::eExclusive;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
		image_info.extent = vk::Extent3D(dim, dim, 1);

		vk::UniqueImage image = ctx.m_device.createImageUnique(image_info);

		vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(image.get());
		vk::MemoryAllocateInfo memory_alloc_info;
		memory_alloc_info.allocationSize = memory_request.size;
		memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::UniqueDeviceMemory image_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
		ctx.m_device.bindImageMemory(image.get(), image_memory.get(), 0);

		// View
		vk::ImageViewCreateInfo view_info;
		view_info.viewType = vk::ImageViewType::e2D;
		view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
		view_info.flags = vk::ImageViewCreateFlags();
		view_info.format = image_info.format;
		view_info.image = image.get();
		view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.layerCount = 1;
		view_info.subresourceRange.levelCount = 1;

		// Sampler
		vk::SamplerCreateInfo sampler_info;
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.compareOp = vk::CompareOp::eNever;
		sampler_info.minLod = 0.f;
		sampler_info.maxLod = 0.f;
		sampler_info.maxAnisotropy = 1.0;
		sampler_info.anisotropyEnable = VK_FALSE;

		m_brgf_lut.m_view = ctx.m_device.createImageViewUnique(view_info);
		m_brgf_lut.m_sampler = ctx.m_device.createSamplerUnique(sampler_info);
		m_brgf_lut.m_image = std::move(image);
		m_brgf_lut.m_memory = std::move(image_memory);

		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.objectHandle = reinterpret_cast<uint64_t&>(m_brgf_lut.m_image.get());
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = to_str(m_brgf_lut.m_image.get());
		ctx.m_device.setDebugUtilsObjectNameEXT(name_info);

		// FB, Att, RP, Pipe, etc.
		{
			// レンダーパス
			// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			// Use subpass dependencies for layout transitions
			std::array<vk::SubpassDependency, 2> dependencies;
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

			// Create the actual renderpass
			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eDontCare)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eUndefined)
				.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setDependencies(dependencies);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);


			auto m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);

			vk::ImageView view[] =
			{
				m_brgf_lut.m_view.get(),
			};

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_renderpass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(dim);
			framebuffer_info.setHeight(dim);
			framebuffer_info.setLayers(1);
			auto m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);

			// pipeline layout
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			auto PL = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);


			// Pipeline
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"BRDFLUT_Make.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"BRDFLUT_Make.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
			vk::Viewport viewport[] = { vk::Viewport(0.f, 0.f, dim, dim, 0.f, 1.f) };
			vk::Rect2D scissor[] = { vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(dim, dim)) };
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);


			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);


			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(array_size(blend_state));
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_size(shaderStages))
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(PL.get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			auto pipeline = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

			// Render
			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_renderpass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(dim, dim)));
			begin_render_Info.setFramebuffer(m_framebuffer.get());

			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
			cmd.draw(3, 1, 0, 0);
			cmd.endRenderPass();

			sDeleter::Order().enque(std::move(m_renderpass), std::move(m_framebuffer), std::move(PL), std::move(pipeline));
		}
	}
};

struct TLAS
{
	vk::UniqueAccelerationStructureKHR m_AS;
	btr::BufferMemory m_AS_buffer;
	//		btr::BufferMemoryEx<vk::AccelerationStructureInstanceKHR> m_instance_buffer;
	vk::AccelerationStructureBuildSizesInfoKHR m_size_info;

};
struct Environment
{
	enum Target 
	{
		Target_IRRADIANCE,
		Target_PREFILTEREDENV,
		Target_Max,
	};
	struct PushBlockIrradiance
	{
		mat4 mvp;
		float deltaPhi = glm::two_pi<float>() / 180.0f;
		float deltaTheta = glm::half_pi<float>() / 64.0f;
	} pushBlockIrradiance;

	struct PushBlockPrefilterEnv
	{
		mat4 mvp;
		float roughness;
		uint32_t numSamples = 32u;
	} pushBlockPrefilterEnv;

	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS;
	vk::UniquePipelineLayout m_pipeline_layout;

	struct Offscreen
	{
		vk::ImageCreateInfo m_imageCI;
		vk::ImageViewCreateInfo m_vewCI;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueRenderPass m_renderpass;
		vk::UniqueFramebuffer m_framebuffer;
	};

	std::array<Offscreen, Target_Max> m_offscreen;
	std::array<vk::UniquePipeline, Target_Max> m_pipeline;

	Environment(btr::Context& ctx, vk::CommandBuffer cmd)
	{
		for (uint32_t target = 0; target < Target_Max; target++)
		{
			int32_t dim = 512;
			auto format = vk::Format::eR32G32B32A32Sfloat;
			switch (target)
			{
			case Target_IRRADIANCE: dim = 64; break;
			case Target_PREFILTEREDENV: dim = 512; break;
			};
			auto& offscreen = m_offscreen[target];

			// Create target cubemap
			{
				// Image
				vk::ImageCreateInfo imageCI;
				imageCI.imageType = vk::ImageType::e2D;
				imageCI.format = format;
				imageCI.extent.width = dim;
				imageCI.extent.height = dim;
				imageCI.extent.depth = 1;
				imageCI.mipLevels = 1;
				imageCI.arrayLayers = 1;
				imageCI.samples = vk::SampleCountFlagBits::e1;
				imageCI.tiling = vk::ImageTiling::eOptimal;
				imageCI.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
				imageCI.flags = {};

				offscreen.m_image = ctx.m_device.createImageUnique(imageCI);

				vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(offscreen.m_image.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				offscreen.m_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
				ctx.m_device.bindImageMemory(offscreen.m_image.get(), offscreen.m_memory.get(), 0);

				// View
				vk::ImageViewCreateInfo viewCI{};
				viewCI.viewType = vk::ImageViewType::e2D;
				viewCI.format = format;
				viewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				viewCI.subresourceRange.levelCount = 1;
				viewCI.subresourceRange.layerCount = 1;
				viewCI.image = offscreen.m_image.get();
				offscreen.m_view = ctx.m_device.createImageViewUnique(viewCI);

				offscreen.m_imageCI = imageCI;
				offscreen.m_vewCI = viewCI;

				vk::ImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.image = offscreen.m_image.get();
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
				imageMemoryBarrier.srcAccessMask = {};
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
				imageMemoryBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });

				vk::DebugUtilsObjectNameInfoEXT name_info;
				name_info.objectHandle = reinterpret_cast<uint64_t&>(offscreen.m_image.get());
				name_info.objectType = vk::ObjectType::eImage;
				name_info.pObjectName = to_str(offscreen.m_image.get());
				ctx.m_device.setDebugUtilsObjectNameEXT(name_info);
			}

			// framebuffer
			{
				// sub pass
				vk::AttachmentReference color_ref[] =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};

				// Use subpass dependencies for layout transitions
				std::array<vk::SubpassDependency, 2> dependencies;
				dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
				dependencies[0].dstSubpass = 0;
				dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
				dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
				dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
				dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
				dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
				dependencies[1].srcSubpass = 0;
				dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
				dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
				dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
				dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
				dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
				dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(array_length(color_ref));
				subpass.setPColorAttachments(color_ref);
				subpass.setPDepthStencilAttachment(nullptr);

				vk::AttachmentDescription attach_description[] =
				{
					// color1
					vk::AttachmentDescription()
					.setFormat(format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eDontCare)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(array_length(attach_description));
				renderpass_info.setPAttachments(attach_description);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);
				renderpass_info.setDependencies(dependencies);
				offscreen.m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);

				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(offscreen.m_renderpass.get());
				framebuffer_info.setAttachmentCount(1);
				framebuffer_info.setWidth(dim);
				framebuffer_info.setHeight(dim);
				framebuffer_info.setLayers(1);
				framebuffer_info.setFlags(vk::FramebufferCreateFlagBits::eImageless);

				vk::FramebufferAttachmentImageInfo framebuffer_image_info;
				framebuffer_image_info.setUsage(offscreen.m_imageCI.usage);
				framebuffer_image_info.setLayerCount(offscreen.m_imageCI.arrayLayers);
				framebuffer_image_info.setViewFormatCount(1);
				framebuffer_image_info.setPViewFormats(&format);
				framebuffer_image_info.setWidth(dim);
				framebuffer_image_info.setHeight(dim);
				framebuffer_image_info.setFlags(offscreen.m_imageCI.flags);
				vk::FramebufferAttachmentsCreateInfo framebuffer_attach_info;
				framebuffer_attach_info.setAttachmentImageInfoCount(1);
				framebuffer_attach_info.setPAttachmentImageInfos(&framebuffer_image_info);

				framebuffer_info.setPNext(&framebuffer_attach_info);

				offscreen.m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);

			}


		}

		{
			// Descriptors
			vk::DescriptorSetLayoutBinding setLayoutBinding = vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment, nullptr };
			vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI;
			descriptorSetLayoutCI.pBindings = &setLayoutBinding;
			descriptorSetLayoutCI.bindingCount = 1;
			m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(descriptorSetLayoutCI);

			// Descriptor sets
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			// Pipeline layout
			vk::PushConstantRange pushConstantRange;
			pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			pushConstantRange.size = (uint32_t)glm::max(sizeof(PushBlockIrradiance), sizeof(PushBlockPrefilterEnv));

			vk::PipelineLayoutCreateInfo pipelineLayoutCI;
			pipelineLayoutCI.setLayoutCount = 1;
			pipelineLayoutCI.pSetLayouts = layouts;
			pipelineLayoutCI.pushConstantRangeCount = 1;
			pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
			m_pipeline_layout = ctx.m_device.createPipelineLayoutUnique(pipelineLayoutCI);

			// Pipeline
			vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI;
			inputAssemblyStateCI.topology = vk::PrimitiveTopology::ePointList;

			vk::PipelineRasterizationStateCreateInfo rasterizationStateCI;
			rasterizationStateCI.polygonMode = vk::PolygonMode::eFill;
			rasterizationStateCI.cullMode = vk::CullModeFlagBits::eNone;
			rasterizationStateCI.frontFace = vk::FrontFace::eCounterClockwise;
			rasterizationStateCI.lineWidth = 1.0f;

			vk::PipelineColorBlendAttachmentState blendAttachmentState;
			blendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			blendAttachmentState.blendEnable = VK_FALSE;

			vk::PipelineColorBlendStateCreateInfo colorBlendStateCI;
			colorBlendStateCI.attachmentCount = 1;
			colorBlendStateCI.pAttachments = &blendAttachmentState;

			vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI;
			depthStencilStateCI.depthTestEnable = VK_FALSE;
			depthStencilStateCI.depthWriteEnable = VK_FALSE;
			depthStencilStateCI.depthCompareOp = vk::CompareOp::eLessOrEqual;
			depthStencilStateCI.front = depthStencilStateCI.back;
			depthStencilStateCI.back.compareOp = vk::CompareOp::eAlways;

			vk::PipelineViewportStateCreateInfo viewportStateCI;
			viewportStateCI.viewportCount = 1;
			viewportStateCI.scissorCount = 1;

			vk::PipelineMultisampleStateCreateInfo multisampleStateCI;
			multisampleStateCI.rasterizationSamples = vk::SampleCountFlagBits::e1;

			std::vector<vk::DynamicState> dynamicStateEnables = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
			vk::PipelineDynamicStateCreateInfo dynamicStateCI;
			dynamicStateCI.setDynamicStates(dynamicStateEnables);

			vk::PipelineVertexInputStateCreateInfo vertexInputStateCI;

			std::array<vk::UniqueShaderModule, 4> shader;
			std::array<std::array<vk::PipelineShaderStageCreateInfo, 3>, Target_Max> shaderStages;
			shader[0] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "filtercube.vert.spv");
			shader[1] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "filtercube.geom.spv");
			shader[2] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "irradiancecube.frag.spv");
			shader[3] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "prefilterenvmap.frag.spv");
			shaderStages[Target_IRRADIANCE][0].module = shader[0].get();
			shaderStages[Target_IRRADIANCE][0].stage = vk::ShaderStageFlagBits::eVertex;
			shaderStages[Target_IRRADIANCE][0].pName = "main";
			shaderStages[Target_IRRADIANCE][1].module = shader[1].get();
			shaderStages[Target_IRRADIANCE][1].stage = vk::ShaderStageFlagBits::eGeometry;
			shaderStages[Target_IRRADIANCE][1].pName = "main";
			shaderStages[Target_PREFILTEREDENV][0] = shaderStages[Target_IRRADIANCE][0];
			shaderStages[Target_PREFILTEREDENV][1] = shaderStages[Target_IRRADIANCE][1];
			shaderStages[Target_IRRADIANCE][2].module = shader[2].get();
			shaderStages[Target_IRRADIANCE][2].stage = vk::ShaderStageFlagBits::eFragment;
			shaderStages[Target_IRRADIANCE][2].pName = "main";
			shaderStages[Target_PREFILTEREDENV][2].module = shader[3].get();
			shaderStages[Target_PREFILTEREDENV][2].stage = vk::ShaderStageFlagBits::eFragment;
			shaderStages[Target_PREFILTEREDENV][2].pName = "main";

			vk::GraphicsPipelineCreateInfo pipelineCI;
			pipelineCI.layout = m_pipeline_layout.get();
			pipelineCI.renderPass = m_offscreen[Target_IRRADIANCE].m_renderpass.get();
			pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
			pipelineCI.pVertexInputState = &vertexInputStateCI;
			pipelineCI.pRasterizationState = &rasterizationStateCI;
			pipelineCI.pColorBlendState = &colorBlendStateCI;
			pipelineCI.pMultisampleState = &multisampleStateCI;
			pipelineCI.pViewportState = &viewportStateCI;
			pipelineCI.pDepthStencilState = &depthStencilStateCI;
			pipelineCI.pDynamicState = &dynamicStateCI;
			pipelineCI.stageCount = array_size(shaderStages[Target_IRRADIANCE]);
			pipelineCI.pStages = shaderStages[Target_IRRADIANCE].data();

			m_pipeline[Target_IRRADIANCE] = ctx.m_device.createGraphicsPipelineUnique({}, pipelineCI).value;

			pipelineCI.renderPass = m_offscreen[Target_PREFILTEREDENV].m_renderpass.get();
			pipelineCI.stageCount = array_size(shaderStages[Target_PREFILTEREDENV]);
			pipelineCI.pStages = shaderStages[Target_PREFILTEREDENV].data();
			m_pipeline[Target_PREFILTEREDENV] = ctx.m_device.createGraphicsPipelineUnique({}, pipelineCI).value;

		}

	}

	std::array<AppImage, Target_Max> execute(btr::Context& ctx, vk::CommandBuffer cmd, AppImage& env)
	{
		std::array<AppImage, Target_Max> cubemaps;
		vk::DescriptorImageInfo images[] = {
			env.info(),
		};
		vk::WriteDescriptorSet write[] =
		{
			vk::WriteDescriptorSet().setDstSet(*m_DS).setDstBinding(0).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(array_length(images)).setPImageInfo(images),
		};
		ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);


		for (uint32_t target = 0; target < Target_Max; target++)
		{
			int32_t dim = 512;
			auto format = vk::Format::eR32G32B32A32Sfloat;
			switch (target)
			{
			case Target_IRRADIANCE:
				dim = 64;
				break;
			case Target_PREFILTEREDENV:
				dim = 512;
				break;
			};
			const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

			// Create target cubemap
			auto& cubemap = cubemaps[target];
			{
				// Image
				vk::ImageCreateInfo imageCI;
				imageCI.imageType = vk::ImageType::e2D;
				imageCI.format = format;
				imageCI.extent.width = dim;
				imageCI.extent.height = dim;
				imageCI.extent.depth = 1;
				imageCI.mipLevels = numMips;
				imageCI.arrayLayers = 6;
				imageCI.samples = vk::SampleCountFlagBits::e1;
				imageCI.tiling = vk::ImageTiling::eOptimal;
				imageCI.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
				imageCI.flags = vk::ImageCreateFlagBits::eCubeCompatible;
				cubemap.m_image = ctx.m_device.createImageUnique(imageCI);

				vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(cubemap.m_image.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				cubemap.m_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
				ctx.m_device.bindImageMemory(cubemap.m_image.get(), cubemap.m_memory.get(), 0);

				// View
				vk::ImageViewCreateInfo viewCI{};
				viewCI.viewType = vk::ImageViewType::eCube;
				viewCI.format = format;
				viewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				viewCI.subresourceRange.levelCount = numMips;
				viewCI.subresourceRange.layerCount = 6;
				viewCI.image = cubemap.m_image.get();
				cubemap.m_view = ctx.m_device.createImageViewUnique(viewCI);

				// Sampler
				vk::SamplerCreateInfo sampler_info;
				sampler_info.magFilter = vk::Filter::eLinear;
				sampler_info.minFilter = vk::Filter::eLinear;
				sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
				sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
				sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
				sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
				sampler_info.mipLodBias = 0.0f;
				sampler_info.compareOp = vk::CompareOp::eNever;
				sampler_info.minLod = 0.0f;
				sampler_info.maxLod = (float)numMips;
				sampler_info.maxAnisotropy = 0;
				sampler_info.anisotropyEnable = VK_FALSE;
				sampler_info.borderColor = vk::BorderColor::eFloatTransparentBlack;
				sampler_info.unnormalizedCoordinates = VK_FALSE;
				cubemap.m_sampler = ctx.m_device.createSamplerUnique(sampler_info);

				cubemap.m_imageCI = imageCI;
				cubemap.m_vewCI = viewCI;
			}


			auto& offscreen = m_offscreen[target];


			// Render cubemap
			vk::ClearValue clearValues[1];
			clearValues[0].color = vk::ClearColorValue{ std::array<float, 4>{0.0f, 0.0f, 0.2f, 0.0f} };

			vk::RenderPassBeginInfo renderPassBeginInfo;
			renderPassBeginInfo.renderPass = offscreen.m_renderpass.get();
			renderPassBeginInfo.framebuffer = offscreen.m_framebuffer.get();
			renderPassBeginInfo.renderArea.extent.width = offscreen.m_imageCI.extent.width;
			renderPassBeginInfo.renderArea.extent.height = offscreen.m_imageCI.extent.height;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;

			vk::RenderPassAttachmentBeginInfo attachment_begin_info;
			std::array<vk::ImageView, 1> views = { offscreen.m_view.get() };
			attachment_begin_info.setAttachments(views);

			renderPassBeginInfo.pNext = &attachment_begin_info;

			std::vector<glm::mat4> matrices =
			{
				glm::lookAt(vec3(0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, -1.f, 0.f)),
				glm::lookAt(vec3(0.f), vec3( 1.f, 0.f, 0.f), vec3(0.f, -1.f, 0.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, -1.f, 0.f), vec3(0.f, 0.f, 1.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, 1.f, 0.f), vec3(0.f, 0.f, -1.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, -1.f, 0.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, 0.f,  1.f), vec3(0.f, -1.f, 0.f)),
			};

			vk::Viewport viewport;
			viewport.width = (float)dim;
			viewport.height = (float)dim;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vk::Rect2D scissor;
			scissor.extent.width = dim;
			scissor.extent.height = dim;

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = numMips;
			subresourceRange.layerCount = 6;

			// Change image layout for all cubemap faces to transfer destination
			{
				vk::ImageMemoryBarrier imageMemoryBarrier;
				imageMemoryBarrier.image = cubemap.m_image.get();
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				imageMemoryBarrier.srcAccessMask = {};
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[target].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, { m_DS.get() }, {});

			for (uint32_t m = 0; m < numMips; m++)
			{
				for (uint32_t f = 0; f < 6; f++)
				{

					viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
					viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
					cmd.setViewport(0, 1, &viewport);
					cmd.setScissor(0, 1, &scissor);

					// Render scene from cube face's point of view
					cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

					// Pass parameters for current pass using a push constant block
					switch (target)
					{
					case Target_IRRADIANCE:
						pushBlockIrradiance.mvp = glm::ortho(-1.f, 1.f, -1.f, 1.f) * matrices[f];
						cmd.pushConstants(m_pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eGeometry|vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlockIrradiance), & pushBlockIrradiance);
						break;
					case Target_PREFILTEREDENV:
						pushBlockPrefilterEnv.mvp = glm::ortho(-1.f, 1.f, -1.f, 1.f) * matrices[f];
						pushBlockPrefilterEnv.roughness = (float)m / (float)(numMips - 1);
						cmd.pushConstants(m_pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlockPrefilterEnv), &pushBlockPrefilterEnv);
						break;
					};


					cmd.draw(1, 1, 0, 0);

					cmd.endRenderPass();

					{
						vk::ImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.image = offscreen.m_image.get();
						imageMemoryBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
						imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
						imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
						imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
						imageMemoryBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
					}

					// Copy region for transfer from framebuffer to cube face
					vk::ImageCopy copyRegion;

					copyRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
					copyRegion.srcSubresource.baseArrayLayer = 0;
					copyRegion.srcSubresource.mipLevel = 0;
					copyRegion.srcSubresource.layerCount = 1;
					copyRegion.srcOffset = vk::Offset3D{ 0, 0, 0 };

					copyRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
					copyRegion.dstSubresource.baseArrayLayer = f;
					copyRegion.dstSubresource.mipLevel = m;
					copyRegion.dstSubresource.layerCount = 1;
					copyRegion.dstOffset = vk::Offset3D{ 0, 0, 0 };

					copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
					copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
					copyRegion.extent.depth = 1;

					cmd.copyImage(offscreen.m_image.get(), vk::ImageLayout::eTransferSrcOptimal, cubemap.m_image.get(), vk::ImageLayout::eTransferDstOptimal, { copyRegion });

					{
						vk::ImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.image = offscreen.m_image.get();
						imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
						imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
						imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
						imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
						imageMemoryBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
					}
				}
			}

			{
				vk::ImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.image = cubemap.m_image.get();
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
			}
		}

		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.objectHandle = reinterpret_cast<uint64_t&>(cubemaps[0].m_image.get());
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = to_str(cubemaps[0].m_image.get());
		ctx.m_device.setDebugUtilsObjectNameEXT(name_info);
		name_info.objectHandle = reinterpret_cast<uint64_t&>(cubemaps[1].m_image.get());
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = to_str(cubemaps[1].m_image.get());
		ctx.m_device.setDebugUtilsObjectNameEXT(name_info);
		return cubemaps;
	}
};
struct Context
{
	enum DSL
	{
		DSL_Scene,
		DSL_Num,
	};

	std::shared_ptr<btr::Context> m_ctx;
	vk::UniqueDescriptorSetLayout m_DSL[DSL_Num];


	struct RenderConfig
	{
		vec4 LightDir;

		float exposure;
		float gamma;

		int32_t skybox_render_type;
		float lod;
		float ambient_power;

	};

	Environment m_env;
	AppImage m_environment;
	std::array<AppImage, 2> m_environment_filter;

	RenderConfig m_render_config;
	btr::BufferMemoryEx<RenderConfig> u_render_config;

	BRDFLookupTable m_lut;
	AppImage m_environment_debug;

	vk::UniqueDescriptorSet m_DS_Scene;
	vk::UniqueDescriptorSet m_DS_Model;

	Resource m_model_resource;
	TLAS m_TLAS;

	Context(std::shared_ptr<btr::Context>& ctx, vk::CommandBuffer cmd)
		: m_env(*ctx, cmd)
		, m_lut(*ctx, cmd)
		, m_model_resource(*ctx)
	{
		m_ctx = ctx;
		m_render_config.LightDir = vec4(1.f);
		m_render_config.exposure = 10.f;
		m_render_config.gamma = 2.2f;
		m_render_config.skybox_render_type = 0;
		m_render_config.lod = 0.f;
		m_render_config.ambient_power = 1.f;
		u_render_config = ctx->m_uniform_memory.allocateMemory<RenderConfig>(1);
		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eMeshNV;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(13, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(100, vk::DescriptorType::eAccelerationStructureKHR, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Scene] = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}
		}

		// environment texture
		{
			gli::texture_cube tex(gli::load(btr::getResourceAppPath() + "environments/papermill.ktx"));

			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = (vk::Format)tex.format();
			image_info.mipLevels = (uint32_t)tex.levels();
			image_info.arrayLayers = 6;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;
			image_info.extent = vk::Extent3D(tex.extent().x, tex.extent().y, 1);

			vk::UniqueImage image = ctx->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = ctx->m_device.getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			vk::UniqueDeviceMemory image_memory = ctx->m_device.allocateMemoryUnique(memory_alloc_info);
			ctx->m_device.bindImageMemory(image.get(), image_memory.get(), 0);

			auto staging_buffer = ctx->m_staging_memory.allocateMemory<byte>((uint32_t)tex.size(), true);
			memcpy(staging_buffer.getMappedPtr(), tex.data(), tex.size());

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = VK_REMAINING_MIP_LEVELS;
			subresourceRange.levelCount = VK_REMAINING_ARRAY_LAYERS;

			{
				// staging_bufferからimageへコピー
				{
					vk::ImageMemoryBarrier to_copy_barrier;
					to_copy_barrier.image = image.get();
					to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
					to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
					to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_copy_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				}

				std::vector<vk::BufferImageCopy> copys;
				auto offset = staging_buffer.getInfo().offset;
				for (uint32_t face = 0; face < 6; face++)
				{
					for (uint32_t level = 0; level < tex.levels(); level++)
					{
						vk::BufferImageCopy copy = {};
						copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						copy.imageSubresource.mipLevel = level;
						copy.imageSubresource.baseArrayLayer = face;
						copy.imageSubresource.layerCount = 1;
						copy.imageExtent.width = static_cast<uint32_t>(tex[face][level].extent().x);
						copy.imageExtent.height = static_cast<uint32_t>(tex[face][level].extent().y);
						copy.imageExtent.depth = 1;
						copy.bufferOffset = offset;

						copys.push_back(copy);

						// Increase offset into staging buffer for next level / face
						offset += tex[face][level].size();
					}
				}
				cmd.copyBufferToImage(staging_buffer.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copys);

				{
					vk::ImageMemoryBarrier to_shader_read_barrier;
					to_shader_read_barrier.image = image.get();
					to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
					to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
					to_shader_read_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
				}

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::eCube;
			view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eLinear;
			sampler_info.minFilter = vk::Filter::eLinear;
			sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
			sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
			sampler_info.mipLodBias = 0.0f;
			sampler_info.compareOp = vk::CompareOp::eNever;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = (float)image_info.mipLevels;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

			m_environment.m_image = std::move(image);
			m_environment.m_memory = std::move(image_memory);
			m_environment.m_view = ctx->m_device.createImageViewUnique(view_info);
			m_environment.m_sampler = ctx->m_device.createSamplerUnique(sampler_info);

			m_environment_filter = m_env.execute(*ctx, cmd, m_environment);

		}

		// debug cube map
		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR32G32B32A32Sfloat;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 6;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;
			image_info.extent = vk::Extent3D(1, 1, 1);

			vk::UniqueImage image = ctx->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = ctx->m_device.getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			vk::UniqueDeviceMemory image_memory = ctx->m_device.allocateMemoryUnique(memory_alloc_info);
			ctx->m_device.bindImageMemory(image.get(), image_memory.get(), 0);

			vec4 data[] = 
			{
				vec4(1.f, 0.f, 0.f, 1.f),
				vec4(0.f, 1.f, 0.f, 1.f),
				vec4(0.f, 0.f, 1.f, 1.f),
				vec4(1.f, 1.f, 0.f, 1.f),
				vec4(0.f, 1.f, 1.f, 1.f),
				vec4(1.f, 0.f, 1.f, 1.f),
			};
			auto staging_buffer = ctx->m_staging_memory.allocateMemory<vec4>(6, true);
			memcpy(staging_buffer.getMappedPtr(), data, sizeof(data));

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = VK_REMAINING_MIP_LEVELS;
			subresourceRange.levelCount = VK_REMAINING_ARRAY_LAYERS;

			{
				// staging_bufferからimageへコピー
				{
					vk::ImageMemoryBarrier to_copy_barrier;
					to_copy_barrier.image = image.get();
					to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
					to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
					to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_copy_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				}
				std::vector<vk::BufferImageCopy> copys;
				auto offset = staging_buffer.getInfo().offset;
				for (uint32_t face = 0; face < 6; face++)
				{
					for (uint32_t level = 0; level < image_info.mipLevels; level++)
					{
						vk::BufferImageCopy copy = {};
						copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						copy.imageSubresource.mipLevel = level;
						copy.imageSubresource.baseArrayLayer = face;
						copy.imageSubresource.layerCount = 1;
						copy.imageExtent.width = 1;
						copy.imageExtent.height = 1;
						copy.imageExtent.depth = 1;
						copy.bufferOffset = offset;

						copys.push_back(copy);

						offset += sizeof(vec4);
					}
				}
				cmd.copyBufferToImage(staging_buffer.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copys);


				{
					vk::ImageMemoryBarrier to_shader_read_barrier;
					to_shader_read_barrier.image = image.get();
					to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
					to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
					to_shader_read_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
				}

			}

			vk::ImageViewCreateInfo view_info;
			view_info.viewType = vk::ImageViewType::eCube;
			view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eNearest;
			sampler_info.minFilter = vk::Filter::eNearest;
			sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
			sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
			sampler_info.mipLodBias = 0.0f;
			sampler_info.compareOp = vk::CompareOp::eNever;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = 0.f;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

			m_environment_debug.m_image = std::move(image);
			m_environment_debug.m_memory = std::move(image_memory);
			m_environment_debug.m_view = ctx->m_device.createImageViewUnique(view_info);
			m_environment_debug.m_sampler = ctx->m_device.createSamplerUnique(sampler_info);
		}
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[Context::DSL_Scene].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS_Scene = std::move(ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			{
 				vk::DescriptorBufferInfo uniforms[] =
 				{
					u_render_config.getInfo()
 				};

				vk::DescriptorImageInfo images[] = 
				{
					m_lut.m_brgf_lut.info(),
					m_environment.info(),
					m_environment_filter[0].info(),
					m_environment_filter[1].info(),
				};
				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet().setDstSet(*m_DS_Scene).setDstBinding(0).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(array_length(uniforms)).setPBufferInfo(uniforms),
					vk::WriteDescriptorSet().setDstSet(*m_DS_Scene).setDstBinding(10).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(array_length(images)).setPImageInfo(images)
				};
				ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

			}
		}

	}




	void execute(vk::CommandBuffer cmd)
	{
		app::g_app_instance->m_window->getImgui()->pushImguiCmd([this]()
			{
				static bool is_open;
				ImGui::SetNextWindowSize(ImVec2(150.f, 300.f));
				ImGui::Begin("RenderConfig", &is_open, ImGuiWindowFlags_NoSavedSettings);
				ImGui::DragFloat4("Light Dir", &m_render_config.LightDir[0], 0.01f, -1.f, 1.f);
				ImGui::SliderFloat("exposure", &this->m_render_config.exposure, 0.0f, 20.f);

				ImGui::Separator();
				const char* types[] = { "normal", "irradiance", "prefiltered", };
				ImGui::Combo("Skybox", &m_render_config.skybox_render_type, types, array_size(types));
				ImGui::SliderFloat("lod", &m_render_config.lod, 0.f, 1.f);
				ImGui::SliderFloat("ambient power", &m_render_config.ambient_power, 0.f, 1.f);

				ImGui::End();

			});
		auto staging = m_ctx->m_staging_memory.allocateMemory<RenderConfig>(1, true);
		memcpy_s(staging.getMappedPtr(), sizeof(RenderConfig), &m_render_config, sizeof(RenderConfig));
		vk::BufferCopy copy = vk::BufferCopy(staging.getInfo().offset, u_render_config.getInfo().offset, staging.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, u_render_config.getInfo().buffer, copy);

		{
			vk::BufferMemoryBarrier to_read = u_render_config.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, {}, {}, { to_read }, { });
		}
	}
	void execute_tlas(vk::CommandBuffer cmd, btr::BufferMemoryEx<vk::AccelerationStructureInstanceKHR>& instances)
	{
		TLAS old = std::move(m_TLAS);
		auto& ctx = *m_ctx;

		vk::AccelerationStructureGeometryKHR TLASGeom;
		TLASGeom.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
		TLASGeom.geometryType = vk::GeometryTypeKHR::eInstances;
		vk::AccelerationStructureGeometryInstancesDataKHR instance_data;
		instance_data.data.deviceAddress = instances.getDeviceAddress();
		TLASGeom.geometry.instances = instance_data;

		vk::AccelerationStructureBuildGeometryInfoKHR TLAS_BI;
		TLAS_BI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		TLAS_BI.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
		TLAS_BI.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		TLAS_BI.geometryCount = 1;
		TLAS_BI.pGeometries = &TLASGeom;

		auto blas_count = instances.getDescriptor().element_num;
		auto size_info = ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, TLAS_BI, blas_count);

		auto AS_buffer = ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
		vk::AccelerationStructureCreateInfoKHR accelerationCI;
		accelerationCI.type = vk::AccelerationStructureTypeKHR::eTopLevel;
		accelerationCI.buffer = AS_buffer.getInfo().buffer;
		accelerationCI.offset = AS_buffer.getInfo().offset;
		accelerationCI.size = AS_buffer.getInfo().range;
		m_TLAS.m_AS = ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);
		TLAS_BI.dstAccelerationStructure = m_TLAS.m_AS.get();

		auto scratch_buffer = ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize, true);
		TLAS_BI.scratchData.deviceAddress = scratch_buffer.getDeviceAddress();

		vk::AccelerationStructureBuildRangeInfoKHR range;
		range.primitiveCount = 1;
		range.primitiveOffset = 0;
		range.firstVertex = 0;
		range.transformOffset = 0;

		cmd.buildAccelerationStructuresKHR({ TLAS_BI }, { &range });
		m_TLAS.m_AS_buffer = std::move(AS_buffer);

		sDeleter::Order().enque(std::move(old), std::move(scratch_buffer));
	}
};
struct Skybox
{
	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_PL;

	vk::UniqueRenderPass m_renderpass;
	vk::UniqueFramebuffer m_framebuffer;

	Skybox(Context& ctx_, vk::CommandBuffer cmd, RenderTarget& rt)
	{
		auto& ctx = *ctx_.m_ctx;
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					ctx_.m_DSL[Context::DSL_Scene].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// graphics pipeline render
		{
			// レンダーパス
			{
				// sub pass
				vk::AttachmentReference color_ref[] =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(1);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(array_length(color_ref));
				subpass.setPColorAttachments(color_ref);
				subpass.setPDepthStencilAttachment(&depth_ref);

				vk::AttachmentDescription attach_description[] =
				{
					// color1
					vk::AttachmentDescription()
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					// depth
					vk::AttachmentDescription()
					.setFormat(rt.m_depth_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(array_length(attach_description));
				renderpass_info.setPAttachments(attach_description);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);
				m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);
			}

			{
				vk::ImageView view[] =
				{
					rt.m_view,
					rt.m_depth_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_renderpass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(rt.m_info.extent.width);
				framebuffer_info.setHeight(rt.m_info.extent.height);
				framebuffer_info.setLayers(1);
				m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Skybox.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"Skybox.geom.spv", vk::ShaderStageFlagBits::eGeometry},
				{"Skybox.frag.spv", vk::ShaderStageFlagBits::eFragment},

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
			vk::Viewport viewport[] = { vk::Viewport(0.f, 0.f, (float)rt.m_resolution.width, (float)rt.m_resolution.height, 0.f, 1.f) };
			vk::Rect2D scissor[] = { vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution) };
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


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
				.setColorWriteMask(vk::ColorComponentFlagBits::eR| vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(array_size(blend_state));
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_size(shaderStages))
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL.get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}

	}

	void execute_Render(vk::CommandBuffer cmd, Context& ctx, RenderTarget& rt)
	{
		DebugLabel _label(cmd, __FUNCTION__);
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

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 1, { ctx.m_DS_Scene.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.draw(1, 1, 0, 0);
		cmd.endRenderPass();

	}
};

#include <020_RT/Renderer.h>
int main()
{

	{
		uint32_t a = ~0;
		uint32_t b = ~0;
		auto c = a + b;
		auto c2 = (a + b) < ~0;
		auto A = 127;
		auto B = 9217;
		auto C = glm::findMSB(A ^ B);
		uint32_t primitiveBits = 1;
		uint32_t maxBlockBits = ~0;
		uint32_t primBits = (200 - 1) * 3 * primitiveBits;
		uint32_t vertBits = (300 - 1) * 571925;
		bool     state = (primBits + vertBits) <= maxBlockBits;
		int _ = 0;
	}
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-0.2f, 3.1f, 0.02f);
	camera->getData().m_target = vec3(-0.2f, 1.1f, -0.03f);
	camera->getData().m_up = vec3(0.f, 0.f, 1.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);
	auto ctx = std::make_shared<Context>(context, setup_cmd);


	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	DrawHelper draw_helper{ context };

//	std::shared_ptr<gltf::gltfResource> model = ctx->m_model_resource.LoadScene(*context, setup_cmd, btr::getResourceAppPath() + "pbr/DamagedHelmet.gltf");
	std::shared_ptr<gltf::gltfResource> model = ctx->m_model_resource.LoadScene(*context, setup_cmd, btr::getResourceAppPath() + "Sponza/Sponza.gltf");

	Renderer renderer(*ctx, *app.m_window->getFrontBuffer());
	Skybox skybox(*ctx, setup_cmd, *app.m_window->getFrontBuffer());

	btr::BufferMemoryEx<vk::AccelerationStructureInstanceKHR> instance_buffer;
	{
		vk::TransformMatrixKHR transform_matrix =
		{
			std::array<std::array<float, 4>, 3>
			{
				std::array<float, 4>{1.f, 0.f, 0.f, 0.f},
				std::array<float, 4>{0.f, 1.f, 0.f, 0.f},
				std::array<float, 4>{0.f, 0.f, 1.f, 0.f},
			}
		};

		std::array<vk::AccelerationStructureInstanceKHR, 2> instance;
		auto& instance1 = instance[0];
		instance1.transform = transform_matrix;
		instance1.instanceCustomIndex = 0;
		instance1.mask = 0xFF;
		instance1.instanceShaderBindingTableRecordOffset = 0;
		instance1.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance1.accelerationStructureReference = context->m_device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(model->m_BLAS.m_AS.get()));

		vk::TransformMatrixKHR transform_matrix2 =
		{
			std::array<std::array<float, 4>, 3>
			{
				std::array<float, 4>{0.5f, 0.f, 0.f, 2.f},
				std::array<float, 4>{0.f, 0.5f, 0.f, 0.f},
				std::array<float, 4>{0.f, 0.f, 0.5f, 0.f},
			}
		};

		auto& instance2 = instance[1];
		instance2.transform = transform_matrix2;
		instance2.instanceCustomIndex = 0;
		instance2.mask = 0xFF;
		instance2.instanceShaderBindingTableRecordOffset = 0;
		instance2.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance2.accelerationStructureReference = context->m_device.getAccelerationStructureAddressKHR(vk::AccelerationStructureDeviceAddressInfoKHR(model->m_BLAS.m_AS.get()));

	
		instance_buffer = context->m_storage_memory.allocateMemory<vk::AccelerationStructureInstanceKHR>(2, true);

		setup_cmd.updateBuffer<vk::AccelerationStructureInstanceKHR>(instance_buffer.getInfo().buffer, instance_buffer.getInfo().offset, instance);
		vk::BufferMemoryBarrier barrier[] = {
			instance_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
		};
		setup_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});
	}
	ctx->execute_tlas(setup_cmd, instance_buffer);

	struct ObjectData
	{
		mat4 worldMatrix;
		mat4 worldMatrixIT;
	};
	btr::BufferMemoryEx<ObjectData> object_buffer;
	{
		std::array<ObjectData, 2> obj;

		obj[0].worldMatrix = glm::translate(mat4(1.f), vec3(0.f));
		obj[1].worldMatrix = glm::translate(mat4(0.5f), vec3(1.f));
		for (auto& o : obj )
		{
			o.worldMatrixIT = glm::inverseTranspose(o.worldMatrix);
		}

		object_buffer = context->m_storage_memory.allocateMemory<ObjectData>(2);

		setup_cmd.updateBuffer<ObjectData>(object_buffer.getInfo().buffer, object_buffer.getInfo().offset, obj);
		vk::BufferMemoryBarrier barrier[] = {
			object_buffer.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		setup_cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTaskShaderNV, {}, {}, { array_size(barrier), barrier }, {});
	}


	btr::BufferMemoryEx<IndirectCmd> b_render_cmd = context->m_vertex_memory.allocateMemory<IndirectCmd>(1);
	std::array<IndirectCmd, 1> render_cmd;
 	render_cmd[0].task = model->m_mesh[0].m_primitive[0].m_task;
 	render_cmd[0].PrimitiveAddress = model->m_mesh[0].b_primitive.getDeviceAddress();
	setup_cmd.updateBuffer<IndirectCmd>(b_render_cmd.getInfo().buffer, b_render_cmd.getInfo().offset, render_cmd);
	btr::BufferMemoryEx<uint32_t> b_cmd_counter = context->m_vertex_memory.allocateMemory<uint32_t>(1);
	setup_cmd.fillBuffer(b_cmd_counter.getInfo().buffer, b_cmd_counter.getInfo().offset, b_cmd_counter.getInfo().range, 1u);

	app.setup();
	context->m_device.waitIdle();

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
					ctx->execute(cmd);
					skybox.execute_Render(cmd, *ctx, *render_target);
					renderer.execute_Render(cmd, *render_target, *model);
//					renderer.execute_Render(cmd, *render_target, b_render_cmd.getInfo(), b_cmd_counter.getInfo(), object_buffer.getInfo());
//					draw_helper.draw(*context, cmd, *render_target, ctx->m_lut.m_brgf_lut);
//					draw_helper.draw_texcube(*context, cmd, *render_target, ctx->m_environment_filter[0]);
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

