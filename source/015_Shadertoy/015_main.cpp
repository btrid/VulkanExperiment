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
#include <applib/sAppImGui.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/Geometry.h>
#include <glm/gtx/intersect.hpp>
#include "Sky.h"
#include "SkyBase.h"

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "imgui.lib")

struct VertexRender
{
	VertexRender(const std::shared_ptr<btr::Context>& context)
	{
		m_context = context;

		// shader
		{
			const char* name[] =
			{
				"VertexRender.vert.spv",
				"VertexRender.frag.spv",
			};

			std::string path = btr::getResourceShaderPath();
			for (size_t i = 0; i < array_length(name); i++) {
				m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
			}
		}
		// pipeline layout
		{
	//				vk::DescriptorSetLayout layouts[] = {};
			vk::PushConstantRange ranges[] = {
				vk::PushConstantRange().setSize(64).setStageFlags(vk::ShaderStageFlagBits::eVertex),
			};

			vk::PipelineLayoutCreateInfo pipeline_layout_info;
	//				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
	//				pipeline_layout_info.setPSetLayouts(layouts);
			pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
			pipeline_layout_info.setPPushConstantRanges(ranges);
			m_pipeline_layout[0] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
		}

		// レンダーパス
		{
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

			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(vk::Format::eR16G16B16A16Sfloat)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eDontCare)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::ePresentSrcKHR),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_render_pass = context->m_device.createRenderPassUnique(renderpass_info);

			{
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_render_pass.get());
				framebuffer_info.flags = vk::FramebufferCreateFlagBits::eImageless;
				framebuffer_info.setAttachmentCount(1);
//				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(1024);
				framebuffer_info.setHeight(1024);
				framebuffer_info.setLayers(1);

				vk::Format format[] = {vk::Format::eR16G16B16A16Sfloat};
				vk::FramebufferAttachmentImageInfo info;
				info.usage = vk::ImageUsageFlagBits::eColorAttachment;
				info.width = 1024;
				info.height = 1024;
				info.layerCount = 1;
				info.viewFormatCount = array_length(format);
				info.pViewFormats = format;
				vk::FramebufferAttachmentsCreateInfo framebuffer_attach_info;
				framebuffer_attach_info.attachmentImageInfoCount = 1;
				framebuffer_attach_info.pAttachmentImageInfos = &info;

				framebuffer_info.setPNext(&framebuffer_attach_info);

				m_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}
		}

		// pipeline
		{

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info[] =
			{
				vk::PipelineInputAssemblyStateCreateInfo()
				.setPrimitiveRestartEnable(VK_FALSE)
				.setTopology(vk::PrimitiveTopology::eTriangleStrip),
			};

			// viewport
	 			vk::PipelineViewportStateCreateInfo viewport_info;
	 			viewport_info.setViewportCount(1);
	// 			viewport_info.setPViewports(&viewport);
	 			viewport_info.setScissorCount(1);
	// 			viewport_info.setPScissors(scissor.data());

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);
//			rasterization_info.setRasterizerDiscardEnable()

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
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
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineShaderStageCreateInfo shader_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[0].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[1].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment)
			};

			vk::VertexInputBindingDescription vib[] = 
			{
				vk::VertexInputBindingDescription().setBinding(0).setInputRate(vk::VertexInputRate::eVertex).setStride(sizeof(vec3))
			};
			vk::VertexInputAttributeDescription via[] =
			{
				// pos
				vk::VertexInputAttributeDescription().setBinding(0).setLocation(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
			};
			vk::PipelineVertexInputStateCreateInfo vertex_input_state;
			vertex_input_state.pVertexAttributeDescriptions = via;
			vertex_input_state.vertexAttributeDescriptionCount = array_length(via);
			vertex_input_state.vertexBindingDescriptionCount = array_length(vib);
			vertex_input_state.pVertexBindingDescriptions = vib;

			vk::DynamicState dynamic_state[] =
			{
				vk::DynamicState::eViewport,
				vk::DynamicState::eScissor,
			};
			vk::PipelineDynamicStateCreateInfo dynamic_info;
			dynamic_info.dynamicStateCount = array_length(dynamic_state);
			dynamic_info.pDynamicStates = dynamic_state;

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info[0])
				.setPViewportState(&viewport_info)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[0].get())
				.setRenderPass(m_render_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info)
				.setPVertexInputState(&vertex_input_state)
				.setPDynamicState(&dynamic_info),

			};
			auto pipelines = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
			m_pipeline[0] = std::move(pipelines[0]);
		}
	}

	std::shared_ptr<btr::Context> m_context;
	std::array<vk::UniqueShaderModule, 2> m_shader;
	std::array<vk::UniquePipelineLayout, 1> m_pipeline_layout;
	std::array<vk::UniquePipeline, 1> m_pipeline;

	vk::UniqueRenderPass m_render_pass;
	vk::UniqueFramebuffer m_framebuffer;

};

int intersectRayAtom(vec3 Pos, vec3 Dir, vec3 AtomPos, vec2 Area, vec4& OutDist)
{
	vec3 RelativePos = AtomPos - Pos;
	float tca = dot(RelativePos, Dir);

	vec2 RadiusSq = Area * Area;
	float d2 = dot(RelativePos, RelativePos) - tca * tca;
	bvec2 intersect = greaterThanEqual(RadiusSq, vec2(d2));
	vec4 dist = vec4(tca) + vec4(glm::sqrt(RadiusSq.yxxy() - d2)) * vec4(-1., -1., 1., 1.);

	int count = 0;
	vec4 rays = vec4(-1.f);
	if (intersect.x && dist.y >= 0.f)
	{
		rays[count*2] = glm::max(dist.x, 0.f);
		rays[count*2+1] = dist.y;
		count++;
	}
	if (intersect.y && dist.w >= 0.f)
	{
		rays[count*2] = intersect.x ? glm::max(dist.z, 0.f) : glm::max(dist.x, 0.f);
		rays[count*2+1] = dist.w;
		count++;
	}
	OutDist = rays;
	return count;

}

std::array<vec3, 6> precomputeNoiseKernel(vec3 lightDirection)
{
	std::array<vec3, 6> g_noise_kernel;

	vec3 up = vec3(0., 1., 0.);
	vec3 side = cross(lightDirection, normalize(up));
	side = dot(side, side) < 0.000001 ? vec3(1., 0., 0.) : normalize(side);
	up = normalize(cross(side, lightDirection));

	g_noise_kernel[0] = lightDirection * 0.5f + side * 0.3f + up * 0.3f;
	g_noise_kernel[1] = lightDirection * 1.0f + side * 0.7f + up * -0.7f;
	g_noise_kernel[2] = lightDirection * 1.5f + side * -0.1f + up * 1.3f;
	g_noise_kernel[3] = lightDirection * 2.0f + side * -1.1f + up * -0.3f;
	g_noise_kernel[4] = lightDirection * 2.5f + side * 1.3f + up * 0.3f;
	g_noise_kernel[5] = lightDirection * 3.0f + side * -0.4f + up * -1.1f;
	return g_noise_kernel;
}

const float u_planet_radius = 1000.;
const float u_planet_cloud_begin = 50.;
const float u_planet_cloud_end = 50. + 32.;
const vec4 u_planet = vec4(0., -u_planet_radius, 0, u_planet_radius);
const vec4 u_cloud_inner = u_planet + vec4(0., 0., 0., u_planet_cloud_begin);
const vec4 u_cloud_outer = u_planet + vec4(0., 0., 0., u_planet_cloud_end);
const float u_cloud_area_inv = 1. / (u_planet_cloud_end - u_planet_cloud_begin);
const float u_mapping = 1. / u_cloud_outer.w;

float heightFraction(const vec3& pos)
{
	return (distance(pos, u_cloud_inner.xyz()) - u_cloud_inner.w)*u_cloud_area_inv;
}
vec3 hlsl_mod(vec3 x, vec3 y) { return x - y * trunc(x / y); }
vec3 _wn_rand(ivec4 _co)
{
	vec4 co = vec4(_co);
	vec3 s = vec3(dot(co.xyz()*9.63f + 53.81f, vec3(12.98, 78.23, 15.61)), dot(co.zxy()*54.53f + 37.33f, vec3(91.87, 47.73, 13.78)), dot(co.yzx()*18.71f + 27.14f, vec3(51.71, 14.35, 24.89)));
	return fract(sin(s) * (float(co.w) + vec3(39.15, 48.51, 67.79)) * vec3(3929.1, 4758.5, 6357.7));
}
float worley_noise2(vec3 invocation, int lod)
{
	invocation *= 20.;
	float value = 0.;
	float total = 0.;
	for (int i = 0; i < lod; i++)
	{
		ivec3 tile_size = max(ivec3(64) >> i, ivec3(1));
		ivec3 tile_id = ivec3(invocation) / tile_size;
		vec3 pos = glm::mod(invocation, vec3(tile_size));

		float _radius = float(tile_size.x) * 0.5;
		tile_id = tile_id + -(1 - ivec3(step(vec3(_radius), pos)));

		float v = 0.;

		for (int z = 0; z < 2; z++)
			for (int y = 0; y < 2; y++)
				for (int x = 0; x < 2; x++)
				{
					ivec3 tid = ivec3(tile_id) + ivec3(x, y, z);
					for (int n = 0; n < 2; n++)
					{
						vec3 p = _wn_rand(ivec4(tid, n))*vec3(tile_size) + vec3(x, y, z)*vec3(tile_size);
						v = glm::max(v, 1.f - glm::min(distance(pos, p) / _radius, 1.f));
					}
				}
		value = value * 2. + v;
		total = total * 2. + 1.;
	}
	return value / total;
}

void noise_test()
{
	constexpr uvec3 reso = uvec3(32, 8, 32);
	vec3 CamPos = vec3(0., 1., 0.);
	vec3 CamDir = normalize(vec3(0., 1., 1.));
	vec3 g_light_foward = normalize(vec3(0., -1., -100.));
	vec3 g_light_up;
	vec3 g_light_side;
	{
		g_light_up = vec3(0., 0., 1.);
		g_light_side = cross(g_light_up, g_light_foward);
		g_light_side = dot(g_light_side, g_light_side) < 0.00001f ? vec3(0., 1., 0.) : normalize(g_light_side);
		g_light_up = cross(g_light_foward, g_light_side);

	}

	for (int z = 0; z < reso.z; z++)
	for (int x = 0; x < reso.x; x++)
	{
		// カメラ位置の作成
		vec2 ndc = (vec2(x, z) + vec2(0.5f, 0.5f)) / vec2(reso.x, reso.z);
		ndc = ndc * 2.f - 1.f;

		CamPos = u_planet.xyz() - CamDir * (u_cloud_outer.w + 3000.f);
		CamPos += (ndc.x*g_light_side + ndc.y*g_light_up) * u_cloud_outer.w;

//		CamPos = vec3(400.);
		CamDir = normalize(-CamPos - vec3(0., 500., 0.));

		// find nearest planet surface point
		vec4 rays;
		int count = intersectRayAtom(CamPos, CamDir, u_planet.xyz(), vec2(u_cloud_inner.w, u_cloud_outer.w), rays);
		int sampleCount = reso.y;
		float transmittance = 1.;
		for (int i = 0; i < count; i++)
		{
			float step = (rays[i * 2 + 1] - rays[i * 2]) / float(sampleCount);
			vec3 pos = CamPos + CamDir * (rays[i * 2] + step * 0.5f);
			for (int y = 0; y < sampleCount; y++)
			{
				worley_noise2(pos, 4);
				pos = pos + CamDir * step;
			}
			break;
		}
	}


}
// https://github.com/erickTornero/realtime-volumetric-cloudscapes
// https://bib.irb.hr/datoteka/949019.Final_0036470256_56.pdf
int main()
{
	noise_test();
	constexpr uvec3 reso = uvec3(32, 1, 32);

	vec3 CamPos = vec3(0., 1., 0.);
	vec3 CamDir = normalize(vec3(0., 1., 1.));
	vec3 g_light_foward = normalize(vec3(0., -1., -100.));
	vec3 g_light_up;
	vec3 g_light_side;
	{
		g_light_up = vec3(0., 0., 1.);
		g_light_side = cross(g_light_up, g_light_foward);
		g_light_side = dot(g_light_side, g_light_side) < 0.00001f ? vec3(0., 1., 0.) : normalize(g_light_side);
		g_light_up = cross(g_light_foward, g_light_side);

	}

	for (int z = 0; z < reso.z; z++)
	for (int x = 0; x < reso.x; x++)
	{
		// カメラ位置の作成
		vec2 ndc = (vec2(x, z) + vec2(0.5f, 0.5f)) / vec2(reso.x, reso.z);
		ndc = ndc * 2.f - 1.f;

		CamPos = u_planet.xyz() - CamDir * (u_cloud_outer.w+3000.f);
		CamPos += (ndc.x*g_light_side + ndc.y*g_light_up) * u_cloud_outer.w;

		CamPos = vec3(400.);
		CamDir = normalize(-CamPos - vec3(0., 500., 0.));

		// find nearest planet surface point
		vec4 rays;
		int count = intersectRayAtom(CamPos, CamDir, u_planet.xyz(), vec2(u_cloud_inner.w, u_cloud_outer.w), rays);
		int sampleCount = reso.y;
		float transmittance = 1.;
		for (int i = 0; i < count; i++)
		{
			float step = (rays[i*2+1] - rays[i*2]) / float(sampleCount);
			vec3 pos = CamPos + CamDir * (rays[i*2]+step*0.5f);
//			for (int y = 0; y < sampleCount; y++)
			{
				vec4 rays;
				if (intersectRayAtom(pos, g_light_foward, u_cloud_inner.xyz(), vec2(u_cloud_inner.w, u_cloud_outer.w), rays) == 0) { continue; }
				float a = rays[1] - rays[0];

				intersectRayAtom(pos, -g_light_foward, u_cloud_inner.xyz(), vec2(u_cloud_inner.w, u_cloud_outer.w), rays);
				float b = rays[1] - rays[0];
				float t = rays[1] / (a + b);

				vec3 p = pos - u_planet.xyz();
				
				vec3 uv = vec3(dot(p, g_light_side), t, dot(p, g_light_up)) * vec3(u_mapping, 1., u_mapping) * vec3(0.5, 1., 0.5) + vec3(0.5, 0., 0.5);
				printf("[%2d,%2d] uv=[%5.3f, %5.3f, %5.3f]\n", x, z, uv.x, uv.y, uv.z);
				pos = pos + CamDir * step;
			}
			break;
		}
	}

	{
		vec3 f = normalize(vec3(1.f));
		vec3 up = vec3(0., 1., 0.);
		vec3 side = cross(f, up);
		side = dot(side, side) < 0.000001 ? vec3(1., 0., 0.) : normalize(side);
		up = normalize(cross(side, f));
		int i = 0;

	}

	{
		const vec3 u_planet = vec3(0.f, 0.f, 0.f);

		vec4 dist;
		int count = 0;
		int c = 1;
#define debug_print printf("pattern%d\n", c); for (int i = 0; i < count; i++) { printf(" %d %5.2f\n", i, dist[i]); printf("   %5.2f\n", dist[i + 1]); } c++
		count = intersectRayAtom(vec3(0.f, 200.f, 0.f), vec3(0.f, -1.f, 0.f), u_planet, vec2(50.f, 150.f), dist); // 宇宙から地上、地上から宇宙へ
		debug_print;
		count = intersectRayAtom(vec3(0.f, 100.f, 0.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); // 大気から地上、地上から宇宙へ
		debug_print;
		count = intersectRayAtom(vec3(0.f, 200.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist);	// 宇宙から地上に当たらず宇宙へ
		debug_print;
		count = intersectRayAtom(vec3(0.f, 100.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); //　大気から地上に当たらず宇宙へ
		debug_print;
		count = intersectRayAtom(vec3(0.f, -50.f, 5.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); // 地上から宇宙へ
		debug_print; 
		count = intersectRayAtom(vec3(0.f, -50.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); // 大気から宇宙へ
		debug_print;
		count = intersectRayAtom(vec3(0.f, -100.f, 5.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist);
		debug_print;
		count = intersectRayAtom(vec3(0.f, -100.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist);
		debug_print;


		int aaa = 0;

	}

	{
		for (int theta = 0; theta < 8; theta++)
		{
			float st = sin((theta + 0.5f) / 8.f * 3.14f);
			float ct = cos((theta + 0.5f) / 8.f * 3.14f);
			for (int phi = 0; phi < 8; phi++)
			{
				float _p = (phi + 0.5f) / 8.f * 6.28f;
				float sp = sin(_p);
				float cp = cos(_p);

				vec3 p = vec3(st*cp, st*sp, ct);
				printf("t,p=[%2d,%2d], p=[%5.3f,%5.3f,%5.3f]\n", theta, phi, p.x, p.y, p.z);
//				p = normalize(vec3(st*cp, st*sp, ct));
//				printf("t,p=[%2d,%2d], p=[%5.3f,%5.3f,%5.3f]\n", theta, phi, p.x, p.y, p.z);
			}

		}
	}

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
	auto& imgui_context = app.m_window_list[0]->getImgui();

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());
	Sky sky(context, app.m_window->getFrontBuffer());
	VertexRender vr(context);
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0, "cmd_skynoise");
		sky.m_skynoise.execute(context, cmd);
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
				cmd_sky,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_sky");
				sky.execute_cpu(imgui_context);
				sky.execute_precompute(cmd, app.m_window->getFrontBuffer());
				sky.executeShadow(cmd, app.m_window->getFrontBuffer());
				sky.execute(cmd, app.m_window->getFrontBuffer());
//				sky.execute_reference(cmd, app.m_window->getFrontBuffer());
//				sky.m_skynoise.execute_Render(context, cmd, app.m_window->getFrontBuffer());

				sAppImGui::Order().Render(cmd);

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

