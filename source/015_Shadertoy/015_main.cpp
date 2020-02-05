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

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "imgui.lib")

float rand(const glm::vec3& co)
{
	return glm::fract(glm::sin(glm::dot(co, glm::vec3(12.98f, 78.23f, 45.41f))) * 43758.5f);
}

float noise(const vec3& pos)
{
	vec3 ip = floor(pos);
	vec3 fp = glm::smoothstep(0.f, 1.f, glm::fract(pos));
	vec2 offset = vec2(0.f, 1.f);
	vec4 a = vec4(rand(ip + offset.xxx), rand(ip + offset.yxx), rand(ip + offset.xyx), rand(ip + offset.yyx));
	vec4 b = vec4(rand(ip + offset.xxy), rand(ip + offset.yxy), rand(ip + offset.xyy), rand(ip + offset.yyy));
	a = mix(a, b, fp.z);
	a.xy = glm::mix(a.xy(), a.zw(), fp.y);
	return glm::mix(a.x, a.y, fp.x);
}

float fBM(const glm::vec3& seed)
{
	float value = 0.f;
	auto p = seed;
	for (int i = 0; i < 4; i++)
	{
		value = value * 2.f + noise(p);
		p *= glm::vec3(2.57859f);
	}
	return value / 15.f;

}
struct Sky 
{
	enum Shader
	{
		Shader_Sky_CS,
		Shader_SkyWithTexture_CS,
		Shader_SkyMakeTexture_CS,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Sky,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_Sky_CS,
		Pipeline_SkyWithTexture_CS,
		Pipeline_SkyMakeTexture_CS,
		Pipeline_Num,
	};

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

	Sky(const std::shared_ptr<btr::Context>& context)
	{
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
			image_info.setExtent(vk::Extent3D(128, 64, 128));
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
			sampler_info.setAnisotropyEnable(false);
			sampler_info.setMagFilter(vk::Filter::eLinear);
			sampler_info.setMinFilter(vk::Filter::eLinear);
			sampler_info.setMinLod(0.f);
			sampler_info.setMaxLod(0.f);
			sampler_info.setMipLodBias(0.f);
			sampler_info.setMipmapMode(vk::SamplerMipmapMode::eNearest);
			sampler_info.setUnnormalizedCoordinates(false);
			m_image_sampler = context->m_device.createSamplerUnique(sampler_info);

// 			vk::ImageMemoryBarrier to_copy_barrier;
// 			to_copy_barrier.image = m_image.get();
// 			to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
// 			to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
// 			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
// 			to_copy_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
// 
// 			vk::ImageMemoryBarrier to_shader_read_barrier;
// 			to_shader_read_barrier.image = m_image.get();
// 			to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
// 			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
// 			to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
// 			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
// 			to_shader_read_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

// 			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
			
// 			{				
// 				auto staging = context->m_staging_memory.allocateMemory<uint8_t>(image_info.extent.width*image_info.extent.height*image_info.extent.depth);
// 				auto* data = staging.getMappedPtr();
// 				for (int z = 0; z < image_info.extent.depth; z++)
// 				for (int y = 0; y < image_info.extent.height; y++)
// 				for (int x = 0; x < image_info.extent.width; x++)
// 				{
// 					glm::vec3 s = glm::vec3(x, y, z) * 0.05f + 2.75f;
// 					auto v = fBM(s);
// 					v *= glm::smoothstep(0.5f, 0.55f, v);
// 					*data = glm::packUnorm1x8(v);
// 					data++;
// 				}
// 
// 				auto copy = vk::BufferImageCopy(staging.getInfo().offset, image_info.extent.width, image_info.extent.height, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0), image_info.extent);
// 				cmd.copyBufferToImage(staging.getInfo().buffer, m_image.get(), vk::ImageLayout::eTransferDstOptimal, copy);
// 			}
//			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });

			vk::ImageMemoryBarrier to_make_barrier;
			to_make_barrier.image = m_image.get();
			to_make_barrier.oldLayout = vk::ImageLayout::eUndefined;
			to_make_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_make_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			to_make_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
			auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_make_barrier });

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
// 				vk::PushConstantRange ranges[] = {
// 					vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
// 				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
// 				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
// 				pipeline_layout_info.setPPushConstantRanges(ranges);
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
				.setLayout(m_pipeline_layout[PipelineLayout_Sky].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Sky].get()),
			};
			auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_Sky_CS] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_SkyWithTexture_CS] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_SkyMakeTexture_CS] = std::move(compute_pipeline[2]);
		}
	}

	void execute(vk::CommandBuffer& cmd, const std::shared_ptr<RenderTarget>& render_target)
	{
		// make texture
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

			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky].get(), 0, array_length(descs), descs, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyMakeTexture_CS].get());

			auto num = app::calcDipatchGroups(uvec3(m_image_info.extent.width, m_image_info.extent.height, m_image_info.extent.depth), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}
		// render_targetÇ…èëÇ≠
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

			vk::DescriptorSet descs[] =
			{
				render_target->m_descriptor.get(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Sky].get(), 0, array_length(descs), descs, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_SkyWithTexture_CS].get());

			auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);

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
	Sky sky(context);
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

