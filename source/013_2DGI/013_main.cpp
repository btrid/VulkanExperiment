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
	virtual void execute() = 0;
};

struct OITRenderer
{
	enum
	{
		RenderWidth = 1024,
		RenderHeight = 1024,
		RenderDepth = 4,
		FragmentBufferSize = RenderWidth*RenderHeight*RenderDepth
	};

	struct Info
	{
		vec3 m_resolution;
		vec3 m_position;
	};
	struct Fragment
	{
		vec3 vertex;
	};

	OITRenderer(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<RenderTarget>& render_target)
	{

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
			m_fragment_link_next_list = context->m_storage_memory.allocateMemory(desc);
		}
		btr::BufferMemoryEx<int32_t> m_fragment_link_next_list;

		{
			auto stage = vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding()
				.setStageFlags(stage)
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
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

			vk::DescriptorBufferInfo storages[] = {
				m_fragment_counter.getInfo(),
				m_fragment_buffer.getInfo(),
				m_fragment_link_next_list.getInfo(),
			};
			vk::WriteDescriptorSet write[] = {
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(0)
				.setDstSet(m_descriptor_set.get())
			};
			context->m_device->updateDescriptorSets(array_length(write), write, 0, nullptr);
		}

		// レンダーパス
		{
			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(0);
			subpass.setPColorAttachments(nullptr);

			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(0);
			renderpass_info.setPAttachments(nullptr);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device->createRenderPassUnique(renderpass_info);
		}
		{
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_render_pass.get());
			framebuffer_info.setAttachmentCount(0);
			framebuffer_info.setPAttachments(nullptr);
			framebuffer_info.setWidth(RenderWidth);
			framebuffer_info.setHeight(RenderHeight);
			framebuffer_info.setLayers(1);

			m_framebuffer = context->m_device->createFramebufferUnique(framebuffer_info);
		}


	}

	vk::CommandBuffer execute(const std::vector<OITPipeline*>& pipeline)
	{
		// begin

		// exe
		for (auto& p : pipeline)
		{
			p->execute();
		}

		// post
	}
	btr::BufferMemoryEx<int32_t> m_fragment_counter;
	btr::BufferMemoryEx<Fragment> m_fragment_buffer;
	btr::BufferMemoryEx<int32_t> m_fragment_link_next_list;
	btr::BufferMemoryEx<Info> m_fragment_info;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

};

struct AppModelOIT : public OITPipeline
{
	AppModelOIT(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<OITRenderer>& renderer)
	{
		{
			const char* name[] =
			{
				"CopyImage.vert.spv",
				"CopyImage.frag.spv",
			};
			static_assert(array_length(name) == array_length(m_shader), "not equal shader num");

			std::string path = btr::getResourceLibPath() + "shader\\binary\\";
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device.getHandle(), path + name[i]);
			}

		}
	}
	void execute()override
	{

	}

	vk::UniqueShaderModule m_shader[2];
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;

};

int main()
{
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 640;
	camera->getData().m_height = 480;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	auto gpu = sGlobal::Order().getGPU(0);
	auto device = sGlobal::Order().getGPU(0).getDevice();

	app::AppDescriptor app_desc;
	app_desc.m_gpu = gpu;
	app_desc.m_window_size = uvec2(640, 480);
	app::App app(app_desc);

	auto context = app.m_context;
	ClearPipeline clear_pipeline(context, app.m_window->getRenderTarget());
	PresentPipeline present_pipeline(context, app.m_window->getRenderTarget(), app.m_window->getSwapchainPtr());


	app.setup();
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum 
			{
				cmd_clear,
				cmd_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			cmds[cmd_clear] = clear_pipeline.execute();
			cmds[cmd_present] = present_pipeline.execute();
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

