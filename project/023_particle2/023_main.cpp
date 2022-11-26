#include <btrlib/Define.h>
#include <cstdlib>
#include <string>
#include <vector>
#include <utility>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <filesystem>
#include <btrlib/cWindow.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/sCameraManager.h>
#include <applib/App.h>
#include <applib/AppPipeline.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
//#pragma comment(lib, "imgui.lib")
#pragma comment(lib, "imgui_node_editor.lib")

#include <applib/sAppImGui.h>

#include "Particle.h"
//template<typename N, typename T>
struct RenderNode
{
	//	constexpr size_t name() { std::hash<N>; }
	//	constexpr size_t hash() { std::hash<std::string>(N); }
};
struct RenderGraph
{
	std::unordered_map<std::string, RenderNode> m_graph;

	template<typename N, typename T, typename... Args>
	constexpr T Add(Args... args)
	{
		return m_graph[N].emplace(args);
	}
};

#include <memory>
#include <filesystem>
#include <spirv_cross/spirv.hpp>
#include <spirv_cross/spirv_reflect.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_cross_parsed_ir.hpp>
#include <spirv_cross/spirv_parser.hpp>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/SPIRV/spirv.hpp>
#include <glslang/SPIRV/spvIR.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/MachineIndependent/localintermediate.h>
#pragma comment(lib, "glslangd.lib")
#pragma comment(lib, "SPIRVd.lib")
#pragma comment(lib, "spirv-cross-glsld.lib")
#pragma comment(lib, "GenericCodeGend.lib")
#pragma comment(lib, "OSDependentd.lib")
#pragma comment(lib, "OGLCompilerd.lib")
#pragma comment(lib, "MachineIndependentd.lib")
#pragma comment(lib, "SPIRV-Toolsd.lib")
#pragma comment(lib, "SPIRV-Tools-diffd.lib")
#pragma comment(lib, "SPIRV-Tools-linkd.lib")
#pragma comment(lib, "SPIRV-Tools-lintd.lib")
#pragma comment(lib, "SPIRV-Tools-optd.lib")
#pragma comment(lib, "SPIRV-Tools-reduce.lib")

#pragma comment(lib, "spirv-cross-cd.lib")
#pragma comment(lib, "spirv-cross-cppd.lib")
#pragma comment(lib, "spirv-cross-glsld.lib")
#pragma comment(lib, "spirv-cross-cored.lib")
#pragma comment(lib, "spirv-cross-reflectd.lib")
#pragma comment(lib, "spirv-cross-utild.lib")
#pragma comment(lib, "SPVRemapperd.lib")

auto particle_ = R"(
struct Particle{

};

Particle Load()
{
	Particle p;

	return p;
}
)";

auto glsl_shader =
R"(
#version 450
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : enable
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_vote : require

#define int32_t int
#define uint32_t uint

struct ParticleChunk
{
	ivec4 
};

struct ModuleInfo 
{
	uint64_t ModuleAddress[128];
};

struct Info{
	uint ParamAddress[32];
};

{EmitterData}

{Module}


layout(local_size_x=128) in;
layout(set=0, binding=0, buffer_reference, scalar) buffer ModuleBuffer { ModuleParam b_module_param[]; };
layout(set=0, binding=1, buffer_reference, scalar) buffer InfoBuffer { Info b_emitter_info[]; };

layout(set=1, binding=0, std140) uniform ModuleData { ModuleData u_module_info; };
layout(set=1, binding=1, buffer_reference, scalar) buffer DataBuffer { Data b_emitter_data[]; };

void main()
{
//	if(gl_GlobalInvocationID.x >= u_module_info.ModuleCount){ return; } 
	ModuleParam param  = ModuleBuffer(u_module_info.ModuleAddress).b_module_param[gl_GlobalInvocationID.x];
	Emitter emitter = EmitterBuffer()

}
)";


std::vector<uint32_t> comileGlslToSpirv(btr::Context& ctx)
{
	glslang::InitializeProcess();
	auto program = std::make_unique<glslang::TProgram>();
	const char* shaderStrings[1];

	EShLanguage stage = EShLangCompute;
	auto shader = std::make_unique<glslang::TShader>(stage);

	//	auto glsl = read_file();

	shaderStrings[0] = glsl_shader;
	shader->setStrings(shaderStrings, 1);

	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

	TBuiltInResource Resources{};
	auto limits = ctx.m_physical_device.getProperties().limits;
	Resources.maxComputeWorkGroupSizeX = limits.maxComputeWorkGroupSize[0];
	Resources.maxComputeWorkGroupSizeY = limits.maxComputeWorkGroupSize[1];
	Resources.maxComputeWorkGroupSizeZ = limits.maxComputeWorkGroupSize[2];
	Resources.limits.doWhileLoops = true;

	if (!shader->parse(&Resources, 150, false, messages)) {
		printf(shader->getInfoLog());
		printf(shader->getInfoDebugLog());
		assert(false);
	}

	program->addShader(shader.get());

	if (!program->link(messages)) {
		printf(shader->getInfoLog());
		printf(shader->getInfoDebugLog());
		assert(false);
	}

	std::vector<uint32_t> spirv_binary;
	spv::SpvBuildLogger logger;
	glslang::GlslangToSpv(*program->getIntermediate(stage), spirv_binary, &logger);
	printf("%s", logger.getAllMessages().c_str());

	glslang::FinalizeProcess();

	return spirv_binary;
}


#include "blueprint.h"

int main()
{
	btr::setResourceAppPath("..\\..\\resource\\023_particle2\\");
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = glm::vec3(-0.3f, 0.0f, -0.3f);
	camera->getData().m_target = glm::vec3(1.f, 0.0f, 1.f);
	camera->getData().m_up = glm::vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 10000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	ClearPipeline clear_render_target(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), context->m_window->getSwapchain());

	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);

	app.setup();

	Blueprint blueprint;
	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum cmds
			{
				cmd_clear,
				cmd_particle,
				cmd_present,
				cmd_num,
			};
			std::vector<vk::CommandBuffer> render_cmds(cmd_num);
			{
				render_cmds[cmd_clear] = clear_render_target.execute();
				render_cmds[cmd_present] = present_pipeline.execute();
			}

			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);

				app::g_app_instance->m_window->getImgui()->pushImguiCmd([&]()
					{
						blueprint.OnFrame(0.016);
					});

				sAppImGui::Order().Render(cmd);
				cmd.end();
				render_cmds[cmd_particle] = cmd;
			}


			app.submit(std::move(render_cmds));
		}
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

}

