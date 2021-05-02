#include <013_2DGI/GI2D/GI2DPath.h>
#include <013_2DGI/PathContext.h>
#include <013_2DGI/GI2D/GI2DDebug.h>
#include <013_2DGI/GI2D/GI2DMakeHierarchy.h>

#include <applib/AppPipeline.h>


int path::path_finding()
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

	gi2d_make_hierarchy.setTarget({ i16vec2(171,171) });

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
				gi2d_make_hierarchy.executeMakeReachMap(cmd, gi2d_path_context);
				gi2d_make_hierarchy.executeDrawReachMap(cmd, gi2d_path_context, app.m_window->getFrontBuffer());

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
