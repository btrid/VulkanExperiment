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
#include <glm/gtx/intersect.hpp>
#include "Sky.h"
#include "SkyBase.h"

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "imgui.lib")


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


// https://github.com/erickTornero/realtime-volumetric-cloudscapes
// https://bib.irb.hr/datoteka/949019.Final_0036470256_56.pdf
int main()
{
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
		// ƒJƒƒ‰ˆÊ’u‚Ìì¬
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
		count = intersectRayAtom(vec3(0.f, 200.f, 0.f), vec3(0.f, -1.f, 0.f), u_planet, vec2(50.f, 150.f), dist); // ‰F’ˆ‚©‚ç’nãA’nã‚©‚ç‰F’ˆ‚Ö
		debug_print;
		count = intersectRayAtom(vec3(0.f, 100.f, 0.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); // ‘å‹C‚©‚ç’nãA’nã‚©‚ç‰F’ˆ‚Ö
		debug_print;
		count = intersectRayAtom(vec3(0.f, 200.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist);	// ‰F’ˆ‚©‚ç’nã‚É“–‚½‚ç‚¸‰F’ˆ‚Ö
		debug_print;
		count = intersectRayAtom(vec3(0.f, 100.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); //@‘å‹C‚©‚ç’nã‚É“–‚½‚ç‚¸‰F’ˆ‚Ö
		debug_print;
		count = intersectRayAtom(vec3(0.f, -50.f, 5.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); // ’nã‚©‚ç‰F’ˆ‚Ö
		debug_print; 
		count = intersectRayAtom(vec3(0.f, -50.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist); // ‘å‹C‚©‚ç‰F’ˆ‚Ö
		debug_print;
		count = intersectRayAtom(vec3(0.f, -100.f, 5.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist);
		debug_print;
		count = intersectRayAtom(vec3(0.f, -100.f, 55.f), vec3(0.f, -1.f, 0.f), u_planet.xyz(), vec2(50.f, 150.f), dist);
		debug_print;


		int aaa = 0;

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
	{
		auto dir = normalize(vec3(1.f, 3.f, 2.f));
		auto rot = normalize(cross(vec3(0.f, 1.f, 0.f), normalize(vec3(dir.x, 0.f, dir.z))));
		int a = 0;
	}
	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);
	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());
	Sky sky(context, app.m_window->getFrontBuffer());

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
				sky.executeShadow(cmd, app.m_window->getFrontBuffer());
				sky.execute(cmd, app.m_window->getFrontBuffer());
//				sky.execute_reference(cmd, app.m_window->getFrontBuffer());
//				sky.m_skynoise.execute_Render(context, cmd, app.m_window->getFrontBuffer());
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

