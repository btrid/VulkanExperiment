#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
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

#include <applib/sAppImGui.h>

#include <013_2DGI/GI2D/GI2DMakeHierarchy.h>
#include <013_2DGI/GI2D/GI2DDebug.h>
#include <013_2DGI/GI2D/GI2DModelRender.h>
#include <013_2DGI/GI2D/GI2DRadiosity.h>
#include <013_2DGI/GI2D/GI2DFluid.h>
#include <013_2DGI/GI2D/GI2DSoftbody.h>

#include <013_2DGI/GI2D/GI2DPhysics.h>
#include <013_2DGI/GI2D/GI2DPhysics_procedure.h>
#include <013_2DGI/GI2D/GI2DVoronoi.h>

#include <013_2DGI/Crowd/Crowd_Procedure.h>
#include <013_2DGI/Crowd/Crowd_CalcWorldMatrix.h>
#include <013_2DGI/Crowd/Crowd_Debug.h>

#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>

#include <013_2DGI/PathContext.h>
#include <013_2DGI/PathSolver.h>

#include <013_2DGI/Game/Game.h>
#include <013_2DGI/Game/Movable.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct Bullet 
{
};
struct Collision
{
	std::shared_ptr<GI2DContext> m_ctx_gi2d;
	std::shared_ptr<CrowdContext> m_ctx_crowd;

};
#include <taskflow/taskflow.hpp>
PathContextCPU pathmake_file()
{
	PathContextCPU::Description desc;
	desc.m_size = ivec2(64);
//	desc.m_start = ivec2(11, 11);
//	desc.m_finish = ivec2(1002, 1000);

	auto aa = ivec2(64, 64) >> 3;
	std::vector<uint64_t> data(aa.x*aa.y);

	FILE* f = nullptr;
	fopen_s(&f, "..\\..\\resource\\testmap.txt", "r");
	char b[64];
	for (int y = 0; y < 64; y++)
	{
		fread_s(b, 64, 1, 64, f);
		for (int x = 0; x < 64; x++)
		{
			switch (b[x]) 
			{
			case '-':
				break;
			case 'X':
				data[x/8 + y/8 * aa.x] |= 1ull << (x%8 + (y%8)*8);
				break;
			case '#':
				desc.m_start = ivec2(x, y);
				break;
			case '*':
				desc.m_finish = ivec2(x, y);
				break;
			}
		}

	}
	PathContextCPU pf(desc);
	pf.m_field = data;
	return pf;
}
#include <glm/gtx/rotate_vector.hpp>
int crowd()
{

	PathContextCPU::Description desc;
	desc.m_size = ivec2(1024);
	desc.m_start = ivec2(11, 11);
	desc.m_finish = ivec2(1002, 1000);
	PathContextCPU pf(desc);
//	pf.m_field = pathmake_maze(1024, 1024);
	pf.m_field = pathmake_noise(1024, 1024);
//	pf = pathmake_file();
	PathSolver solver;
//  	auto solve1 = solver.executeMakeVectorField(pf);
//   	auto solve2 = solver.executeMakeVectorField2(pf);
	//	auto solve = solver.executeSolve(pf);
//	solver.writeConsole(pf);
//	solver.writeSolvePath(pf, solve, "hoge.txt");
//	solver.writeConsole(pf, solve);
//	solver.write(pf, solve2);

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

//	auto appmodel_context = std::make_shared<AppModelContext>(context);

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = uvec2(1024);
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<CrowdContext> crowd_context = std::make_shared<CrowdContext>(context, gi2d_context);
	std::shared_ptr<GI2DPathContext> gi2d_path_context = std::make_shared<GI2DPathContext>(gi2d_context);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	Crowd_Procedure crowd_procedure(crowd_context, gi2d_context, gi2d_sdf_context, gi2d_path_context);
//	Crowd_CalcWorldMatrix crowd_calc_world_matrix(crowd_context, appmodel_context);
	Crowd_Debug crowd_debug(crowd_context);


	gi2d_debug.executeUpdateMap(cmd, pf.m_field),
	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum 
			{
				cmd_model_update,
				cmd_render_clear,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
//				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				crowd_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);
				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
				gi2d_make_hierarchy.executeMakeReachableMap(cmd, gi2d_path_context);
//				gi2d_make_hierarchy.

				crowd_procedure.executeUpdateUnit(cmd);

				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				crowd_procedure.executeRendering(cmd, app.m_window->getFrontBuffer());
				{
//					gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf_context, app.m_window->getFrontBuffer());
//					gi2d_debug.executeDrawReachMap(cmd, gi2d_path_context, app.m_window->getFrontBuffer());
					//					gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				}

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		context->m_device.waitIdle();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;

}

int pathFinding()
{

	PathContextCPU::Description desc;
	desc.m_size = ivec2(1024);
	desc.m_start = ivec2(11, 11);
	desc.m_finish = ivec2(1002, 1000);
	PathContextCPU pf(desc);
//	pf.m_field = pathmake_maze(1024, 1024);
	pf.m_field = pathmake_noise(1024, 1024);
	//	pf = pathmake_file();
//	PathSolver solver;
	//  	auto solve1 = solver.executeMakeVectorField(pf);
	//   	auto solve2 = solver.executeMakeVectorField2(pf);
		//	auto solve = solver.executeSolve(pf);
	//	solver.writeConsole(pf);
	//	solver.writeSolvePath(pf, solve, "hoge.txt");
	//	solver.writeConsole(pf, solve);
	//	solver.write(pf, solve2);

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = uvec2(1024);
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DPathContext> gi2d_path_context = std::make_shared<GI2DPathContext>(gi2d_context);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	gi2d_debug.executeUpdateMap(cmd, pf.m_field),
	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
//				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);
				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);
//				gi2d_make_hierarchy.executeMakeReachMap(cmd, gi2d_path_context);
				gi2d_make_hierarchy.executeMakeReachMap_Multiframe(cmd, gi2d_path_context);
				gi2d_debug.executeDrawReachMap(cmd, gi2d_path_context, app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		context->m_device.waitIdle();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;

}

vec2 nearestAABBPoint(const vec4& aabb, const vec2& p)
{
	return min(max(p, aabb.xy()), aabb.zw());
}


bool containsAABBPoint(const vec4& aabb, const vec2& p)
{
	return p.x >= aabb.x && p.x <= aabb.z
		&& p.y >= aabb.y && p.y <= aabb.w;
}

vec2 intersection(vec4 aabb, vec2 p, vec2 dir)
{
	using glm::max; using glm::min;

	vec4 t = (aabb - p.xyxy()) / dir.xyxy();
	vec2 tmin = min(t.xy(), t.zw());

	return p + max(tmin.x, tmin.y) * dir;
}

vec2 intersectionAABBSegment(vec4 aabb, vec2 p, vec2 dir, vec2 inv_dir)
{
	using namespace glm;
	vec4 t = (aabb - p.xyxy) * inv_dir.xyxy;
	vec2 tmin = min(t.xy(), t.zw());
	vec2 tmax = max(t.xy(), t.zw());

	float n = max(tmin.x, tmin.y);
	float f = min(tmax.x, tmax.y);
//	auto pp = p + (n >= 0. ? n : f) * dir;
	auto pp = p + f * dir;
	printf("out=[%6.3f,%6.3f], p=[%6.3f,%6.3f], d=[%6.3f,%6.3f], nf=[%6.3f,%6.3f]\n", pp.x, pp.y, p.x, p.y, dir.x, dir.y, n, f);
	return pp;
}

vec2 closest(const vec4 seg, const vec2& p)
{
	auto ab = seg.zw() - seg.xy();
	float t = dot(p - seg.xy(), ab) / dot(ab, ab);
	t = glm::clamp(t, 0.f, 1.f);
	return seg.xy() + t * ab;

}

vec2 inetsectLineCircle(const vec2& p, const vec2& d, const vec2& cp)
{
//https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm
	vec2 result = p;
	float t1=0.f, t2=0.f;
	float r = 0.5f;
	float a = dot(d,d);
	float b = 2.f * dot(p-cp, d);
	float c = dot(p-cp, p-cp) - r*r;

	float discriminant = b * b - 4 * a*c;
	float l = distance(p, cp);
	if (distance(p, cp) > r)
	{
		int _ = 0;
	}
	else if (discriminant < 0)
	{
		// no intersection
		int _ = 0;
	}
	else
	{
		// ray didn't totally miss sphere,
		// so there is a solution to
		// the equation.
		discriminant = sqrt(discriminant);

		// either solution may be on or off the ray so need to test both
		// t1 is always the smaller value, because BOTH discriminant and
		// a are nonnegative.
		t1 = (-b - discriminant) / (2.f * a);
		t2 = (-b + discriminant) / (2.f * a);

		// 3x HIT cases:
		//          -o->             --|-->  |            |  --|->
		// Impale(t1 hit,t2 hit), Poke(t1 hit,t2>1), ExitWound(t1<0, t2 hit), 

		// 3x MISS cases:
		//       ->  o                     o ->              | -> |
		// FallShort (t1>1,t2>1), Past (t1<0,t2<0), CompletelyInside(t1<0, t2>1)

		if (t1 >= 0 && t1 <= 1)
		{
			// t1 is the intersection, and it's closer than t2
			// (since t1 uses -b - discriminant)
			// Impale, Poke
//			return true;
		}

		// here t1 didn't intersect so we are either started
		// inside the sphere or completely past it
		if (t2 >= 0 && t2 <= 1)
		{
			// ExitWound
//			return true;
		}

		// no intn: FallShort, Past, CompletelyInside
//		return false;
		if (t1 >= 0.f)
			result = p + d * t1;
		else
			result = p + d * t2;
	}

	printf("out=[%6.3f,%6.3f], p=[%6.3f,%6.3f], cp=[%6.3f,%6.3f], d=[%6.3f,%6.3f], nf=[%6.3f,%6.3f] dist=[%6.3f], %s\n", result.x, result.y, p.x, p.y, cp.x, cp.y, d.x, d.y, t1, t2, l, glm::all(glm::epsilonEqual(p, result, 0.001f))?"not hit":"hit");
	return result;
}


float Signed2DTriArea(vec2 a, vec2 b, vec2 c)
{
	return (a.x - c.x)*(b.y - c.y) - (a.y - c.y)*(b.x - c.x);
}
// Test2DSegmentSegment
bool intersectSegmentSegment(vec2 a, vec2 b, vec2 c, vec2 d, float& t, vec2& p)
{
	float a1 = Signed2DTriArea(a, b, d);
	float a2 = Signed2DTriArea(a, b, c);

	if (a1*a2 < 0.f)
	{
		float a3 = Signed2DTriArea(c, d, a);
		float a4 = a3 + a2 - a1;
		if (a3*a4 < 0.f)
		{
			if (abs((a3 - a4)) < 0.001)
			{
				int aa = 0;
			}
			t = a3 / (a3 - a4);
			p = a + t * (b - a);
			return true;
		}
	}
	return false;
}

bool inetsectSegmentRect(vec2 pos, vec2 dir, vec2 center, vec2 unit)
{
	vec2 u = unit * 0.5f;
	float t=-1.f;
	vec2 p = pos;

	auto c = dot(dir, unit);
	do {
		if (abs(c) > 0.0001)
		{
			if (intersectSegmentSegment(pos, pos + dir, center + vec2(-u.x, -u.y), center + vec2(u.x, -u.y), t, p)) { break; }
			if (intersectSegmentSegment(pos, pos + dir, center + vec2(u.x, u.y), center + vec2(-u.x, u.y), t, p)) { break; }
		}
		if (abs(c) < 0.9999)
		{
			if (intersectSegmentSegment(pos, pos + dir, center + vec2(u.x, -u.y), center + vec2(u.x, u.y), t, p)) { break; }
			if (intersectSegmentSegment(pos, pos + dir, center + vec2(-u.x, u.y), center + vec2(-u.x, -u.y), t, p)) { break; }
		}
	} while (false);

	printf("out=[%6.3f,%6.3f], p=[%6.3f,%6.3f], d=[%6.3f,%6.3f], t=[%6.3f], %s\n", p.x, p.y, pos.x, pos.y, dir.x, dir.y, t, t>=0.f?"not hit":"hit");

	return false;
}

vec2 rotate(vec2 v, float angle) 
{
	float c = cos(angle);
	float s = sin(angle);

	return vec2(v.x * c - v.y * s,
				v.x * s + v.y * c);
}

vec2 rotate(float angle) { return rotate(vec2(1.f, 0.f), angle); }
//vec2 rotate(float angle) { return vec2(cos(angle), sin(angle)); }
int rigidbody()
{
	for (int i = 0; i < 0; i++)
//	while (true)
	{
		vec2 pos = glm::linearRand(vec2(0.f), vec2(1.f));
		vec2 dir = glm::circularRand(1.f);
		float r = glm::linearRand(-3.14f, 3.14f);

		auto unit = rotate(r);
		vec2 sdf = glm::circularRand(1.f);
		inetsectSegmentRect(pos, dir, vec2(0.5f), unit);


		vec2 sdf_r = rotate(sdf, r);
		intersectionAABBSegment(vec4(0.f, 0.f, 1.f, 1.f), pos, sdf_r, 1.f / sdf);



	}
	for (int i = 0; i < 10; i++)
	{
		vec2 sdf = glm::circularRand(1.f);
		vec2 pos = glm::linearRand(vec2(0.f), vec2(1.f));
		vec2 cpos = glm::linearRand(vec2(0.f), vec2(1.f));
		inetsectLineCircle(pos, sdf, cpos);
	}

	{
		auto a = closest(vec4(0.f, 0.f, 1.f, 0.f), vec2(3.f));
		int _ = 0;

	}
	{
		vec2 sdf{1.f, 0.f};
		auto a = vec2(4.5f, 4.5f) - nearestAABBPoint(vec4(4.f, 4.f, 5.f, 5.f), vec2(3.5f, 4.5f));
		auto a1 = sdf * dot(a, sdf);
		auto b = vec2(4.5f, 4.5f) - nearestAABBPoint(vec4(4.f, 4.f, 5.f, 5.f), vec2(4.5f, 3.5f));
		auto b1 = sdf * dot(b, sdf);
		auto c = vec2(4.2f, 4.5f) - nearestAABBPoint(vec4(4.f, 4.f, 5.f, 5.f), vec2(4.8f, 3.8f));
		auto c1 = sdf * dot(c, sdf);
		auto d = vec2(4.2f, 4.9f) - nearestAABBPoint(vec4(4.f, 4.f, 5.f, 5.f), vec2(5.1f, 5.1f));
		auto d1 = sdf * dot(d, sdf);
		int _0 = 0;
	}

	{
// 		vec2 sdf = vec2(-1.f, 0.f);
// 		auto a = intersection(vec4(4.f, 4.f, 5.f, 5.f), vec2(3.5f, 3.8f), vec2(1.f));
// 		auto a1 = intersectionAABBSegment(vec4(4.f, 4.f, 5.f, 5.f), vec2(4.9f, 4.1f), vec2(1.f), 1.f/ vec2(1.f));
// 		//		a -= vec2(0.0001f);
// 		auto b = intersection(vec4(4.f, 4.f, 5.f, 5.f), vec2(5.5f, 5.8f), vec2(-1.f));
// 		auto b1 = intersectionAABBSegment(vec4(4.f, 4.f, 5.f, 5.f), vec2(4.1f, 4.8f), vec2(-1.f), 1.f/vec2(-1.f));
// 		//		b -= vec2(-0.0001f);
// 		auto c = intersection(vec4(4.f, 4.f, 5.f, 5.f), vec2(5.5f, 3.5f), vec2(-1.f, 1.f));
// 		auto c1 = intersectionAABBSegment(vec4(4.f, 4.f, 5.f, 5.f), vec2(4.7f, 4.1f), vec2(-1.f, 1.f), 1.f / vec2(-1.f, 1.f));
		//		c -= vec2(-0.0001f, 0.0001f);


		for (int i = 0; i < 10; i++)
		{
			vec2 sdf = glm::circularRand(1.f);
			vec2 pos = glm::linearRand(vec2(0.f) , vec2(1.f));
			intersectionAABBSegment(vec4(0.f, 0.f, 1.f, 1.f), pos, sdf, 1.f / sdf);
		}

		int _ = 0;
	}
	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = uvec2(1024, 1024);
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<GI2DPhysics> gi2d_physics_context = std::make_shared<GI2DPhysics>(context, gi2d_context);
	std::shared_ptr<GI2DPhysicsDebug> gi2d_physics_debug = std::make_shared<GI2DPhysicsDebug>(gi2d_physics_context, app.m_window->getFrontBuffer());

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DPhysics_procedure gi2d_physics_proc(gi2d_physics_context, gi2d_sdf_context);
	auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

	gi2d_debug.executeMakeFragment(cmd);
//	gi2d_physics_context->executeMakeVoronoi(cmd);
//	gi2d_physics_context->executeMakeVoronoiPath(cmd);
	app.setup();

	while (true)
	{
		cStopWatch time;
		
		app.preUpdate();
		{
			enum
			{
				cmd_model_update,
				cmd_render_clear,
				cmd_crowd,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{

			}

			{
				//cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			// crowd
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				cmd.end();
				cmds[cmd_crowd] = cmd;
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				gi2d_context->execute(cmd);

				if (context->m_window->getInput().m_keyboard.isOn('A'))
				{
 					for (int y = 0; y < 16; y++){
 					for (int x = 0; x < 16; x++){
						GI2DRB_MakeParam param;
						param.aabb = uvec4(220 + x * 32, 620 - y * 16, 8, 8);
						param.is_fluid = false;
						param.is_usercontrol = false;
 						gi2d_physics_context->make(cmd, param);
 					}}
				}

				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
//				gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf_context, app.m_window->getFrontBuffer());

 				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());
//				gi2d_physics_proc.executeDebugDrawCollisionHeatMap(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
			context->m_device.waitIdle();
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

int radiosity_globalline()
{
	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = app_desc.m_window_size;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				//				cmd_model_update,
				cmd_render_clear,
				//				cmd_crowd,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);

				gi2d_Radiosity.executeGlobalLineRadiosity(cmd);
				gi2d_Radiosity.executeGlobalLineRadiosityRendering(cmd);

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

// Bresenham's line algorithm
// https://ja.wikipedia.org/wiki/%E3%83%96%E3%83%AC%E3%82%BC%E3%83%B3%E3%83%8F%E3%83%A0%E3%81%AE%E3%82%A2%E3%83%AB%E3%82%B4%E3%83%AA%E3%82%BA%E3%83%A0
void dda()
{
	ivec2 target(0, 256);
	ivec2 pos0(333, 333);
	ivec2 delta = abs(target - pos0);
	ivec3 _dir = glm::sign(ivec3(target, 0) - ivec3(pos0, 0));

	int axis = delta.x >= delta.y ? 0 : 1;
	ivec2 dir[2];
	dir[0] = _dir.xz();
	dir[1] = _dir.zy();

	{
		int	D = 2 * delta[1 - axis] - delta[axis];
		ivec2 pos = pos0;
		for (; true;)
		{
			if (D > 0)
			{
				pos += dir[1 - axis];
				D = D - 2 * delta[axis];
			}
			else
			{
				pos += dir[axis];
				D = D + 2 * delta[1 - axis];
			}

			printf("pos=[%3d,%3d]\n", pos.x, pos.y);
			if (all(glm::equal(pos, target)))
			{
				break;
			}
		}
		printf("pos\n");
	}
	float dist = distance(vec2(target), vec2(pos0));
	float p = 1. / (1. + dist * dist * 0.01);
	target = vec2(pos0) + vec2(target - pos0) * p * 100.f;
	dist += 1.;
}

#include <glm/gtx/rotate_vector.hpp>
void dda_test()
{
	std::array<int, 32*32> data{};
	ivec3 reso = ivec3(32);
	for (int z = 0; z<32; z++)
	{
		auto ray = glm::rotateZ(glm::vec3(0.f, 1.f, 0.f), glm::radians(180.f) / 32.f*(z + 0.5f));
		ivec2 delta = abs(ray*vec3(reso)).xy();
		ivec3 _dir = ivec3(glm::sign(ray));

		int axis = delta.x >= delta.y ? 0 : 1;
		ivec2 dir[2];
		dir[0] = _dir.xz();
		dir[1] = _dir.zy();

		for (int y = 0; y < reso.y; y++)
		{
			for (int x = 0; x < reso.x; x++)
			{
				int	D = 2 * delta[1-axis] - delta[axis];
				ivec2 pos;
				pos.x=_dir.x>=0?0:reso.x-1;
				pos.y=_dir.y>=0?0:reso.y-1;
				for (; true;)
				{
					if (D > 0)
					{
						pos += dir[1 - axis];
						D = D - 2 * delta[axis];
					}
					else
					{
						pos += dir[axis];
						D = D + 2 * delta[1 - axis];
					}

//					printf("pos=[%3d,%3d]\n", pos.x, pos.y);
					data[y*reso.x + x]++;
					if (any(glm::lessThan(pos, ivec2(0))) || any(glm::greaterThanEqual(pos, reso.xy())))
					{
						break;
					}
				}
			}
		}
	}
	printf("");

}

/* primitive Bresenham's-like algorithm */
void makeCircle(int Ox, int Oy, int R) 
{

	int S = 128;
	std::vector<char> field(S * S);
	int x = R;
	int y = 0;
	int err = 0;
	int dedx = (R << 1) - 1;	// 2x-1 = 2R-1
	int dedy = 1;	// 2y-1

	field[(Ox+R) + Oy * S] = 1;// +0
	field[Ox + (Oy+R) * S] = 1;// +90
	field[(Ox-R) + Oy * S] = 1;// +180
	field[Ox + (Oy-R) * S] = 1;// +270

	while (x > y) 
	{
		y++;
		err += dedy;
		dedy += 2;
		if (err >= dedx) 
		{
			x--;
			err -= dedx;
			dedx -= 2;
		}

		field[(Ox + x) + (Oy + y) * S] = 1;// +theta
		field[(Ox + x) + (Oy - y) * S] = 1;// -theta
		field[(Ox - x) + (Oy + y) * S] = 1;// 180-theta
		field[(Ox - x) + (Oy - y) * S] = 1;// 180+theta
		field[(Ox + y) + (Oy + x) * S] = 1;// 90+theta
		field[(Ox + y) + (Oy - x) * S] = 1;// 90-theta
		field[(Ox - y) + (Oy + x) * S] = 1;// 270+theta
		field[(Ox - y) + (Oy - x) * S] = 1;// 270-theta
	}

	for (int _y = 0; _y < S; _y++)
	{
		for (int _x = 0; _x < S; _x++)
		{
			printf("%c", field[_x + _y * S] ? '@' : ' ');
		}
		printf("\n");
	}
}

int radiosity_rightbased()
{

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = app_desc.m_window_size;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);

				gi2d_Radiosity.executeLightBasedRaytracing(cmd);

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

int radiosity_pixelbased()
{
	
// 	{
// 		for (size_t i = 0; i < 32; i++)
// 		{
// //			float angle_offset = rand(vec2(gl_GlobalInvocationID.xy));
// 			float angle = i * (6.28 / 32.);
// 			vec2 dir = normalize(vec2(cos(angle), sin(angle)));
// 			printf("%i = [%6.2f%6.2f]", i, dir.x, dir.y);
// 			dir = normalize(dir);
// 			printf(", [%6.2f%6.2f]\n", dir.x, dir.y);
// 		}
// 
// 	}
	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = app_desc.m_window_size;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf = std::make_shared<GI2DSDF>(gi2d_context);

	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());

	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

#define USE_SDF
#if defined(USE_SDF)
				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf);
				gi2d_Radiosity.executePixelBasedRaytracing2(cmd, gi2d_sdf);
//				gi2d_make_hierarchy.executeRenderSDF(cmd, gi2d_sdf, app.m_window->getFrontBuffer());
//				gi2d_debug.executeRenderSDF(cmd, gi2d_sdf, app.m_window->getFrontBuffer());
#else
				gi2d_make_hierarchy.executeMakeFragmentMap(cmd);
				gi2d_Radiosity.executePixelBasedRaytracing(cmd);

#endif
				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}



struct Player
{
	DS_GameObject m_gameobject;
};

int main()
{
// 	for(;;)
// 	{ 
//		double a = glm::linearRand(UINT_MAX - 3, UINT_MAX) / double(UINT_MAX);
// 		float a = glm::linearRand((UINT_MAX&0x7fffff) - 3, (UINT_MAX & 0x7fffff)) / float(UINT_MAX & 0x7fffff);
// 		printf("%36.24f\n", a);
// 	}

// 	for (;;)
// 	{
// 		float a = glm::linearRand(-glm::pi<float>(), glm::pi<float>());
// 		float b = glm::linearRand(-glm::pi<float>(), glm::pi<float>());
// 		float d = b - a;
// 		d = d > 3.14f ? 6.28f - d : d;
// 		d = d < -3.14f ? 6.28f + d : d;
// 		printf("a=%8.3f, b=%8.3f, d=%8.3f\n", a, b, d);
// 	}

	dda_test();

	btr::setResourceAppPath("../../resource/");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(0.f, 0.f, 1.f);
	camera->getData().m_target = glm::vec3(0.f, 0.f, 0.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	return pathFinding();
//	return rigidbody();
//	return radiosity_globalline();
//	return radiosity_rightbased();
//	return radiosity_pixelbased();

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());


	GI2DDescription gi2d_desc;
	gi2d_desc.Resolution = app_desc.m_window_size;
	std::shared_ptr<GI2DContext> gi2d_context = std::make_shared<GI2DContext>(context, gi2d_desc);
	std::shared_ptr<GI2DSDF> gi2d_sdf_context = std::make_shared<GI2DSDF>(gi2d_context);
	std::shared_ptr<GI2DPhysics> gi2d_physics_context = std::make_shared<GI2DPhysics>(context, gi2d_context);
	std::shared_ptr<GameContext> game_context = std::make_shared<GameContext>(context, gi2d_context, gi2d_physics_context);
	
	GI2DDebug gi2d_debug(context, gi2d_context);
	GI2DMakeHierarchy gi2d_make_hierarchy(context, gi2d_context);
	GI2DRadiosity gi2d_Radiosity(context, gi2d_context, app.m_window->getFrontBuffer());
	GI2DPhysics_procedure gi2d_physics_proc(gi2d_physics_context, gi2d_sdf_context);
	GameProcedure game_proc(context, game_context);
	Player player;
	{		
		player.m_gameobject = game_context->alloc();

	}

	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		cmd.updateBuffer<uint64_t>(game_context->b_state.getInfo().buffer, game_context->b_state.getInfo().offset, 1ull);
		cmd.updateBuffer<vec2>(game_context->b_movable.getInfo().buffer, game_context->b_movable.getInfo().offset, vec2(756.f, 990.f));

		GI2DRB_MakeParam param;
		param.aabb = uvec4(756, 990, 16, 16);
		param.is_fluid = false;
		param.is_usercontrol = true;
		gi2d_physics_context->make(cmd, param);
		auto info = game_context->b_movable.getInfo();
		info.offset += player.m_gameobject.index * sizeof(cMovable) + offsetof(cMovable, rb_address);
		gi2d_physics_context->getRBID(cmd, info);
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
				cmd_gi2d,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);

			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}

			// gi2d
			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0, "cmd_gi2d");
				if (context->m_window->getInput().m_keyboard.isOn('A'))
				{
					for (int y = 0; y < 1; y++) {
						for (int x = 0; x < 1; x++) {
							GI2DRB_MakeParam param;
							param.aabb = uvec4(880 + x * 32, 620 - y * 16, 64, 64);
//							param.aabb = uvec4(880 + x * 32, 620 - y * 16, 8, 8);
							param.is_fluid = false;
							param.is_usercontrol = false;
							gi2d_physics_context->make(cmd, param);
						}
					}
				}


				gi2d_context->execute(cmd);
				gi2d_debug.executeMakeFragment(cmd);

				gi2d_make_hierarchy.executeMakeFragmentMapAndSDF(cmd, gi2d_sdf_context);
				gi2d_debug.executeDrawFragment(cmd, app.m_window->getFrontBuffer());
 				game_proc.executePlayerUpdate(cmd, context, game_context, player.m_gameobject);
 				game_proc.executeMovableUpdatePrePyhsics(cmd, context, game_context, player.m_gameobject);
 				gi2d_physics_proc.execute(cmd, gi2d_physics_context, gi2d_sdf_context);
 				gi2d_physics_proc.executeDrawParticle(cmd, gi2d_physics_context, app.m_window->getFrontBuffer());
				game_proc.executeMovableUpdatePostPyhsics(cmd, context, game_context, player.m_gameobject);

//				gi2d_debug.executeDrawFragmentMap(cmd, app.m_window->getFrontBuffer());

				cmd.end();
				cmds[cmd_gi2d] = cmd;
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%-6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}
}