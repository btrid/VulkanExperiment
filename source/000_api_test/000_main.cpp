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
#include <btrlib/Define.h>
#include <btrlib/cWindow.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <applib/App.h>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")


void bittest()
{
		// count
	{
		uint64_t a = 0xffffffffffffffff;
		uint64_t c = 0;
		c = (a & 0x5555555555555555ull) + ((a >> 1) & 0x5555555555555555ull);
		c = (c & 0x3333333333333333ull) + ((c >> 2) & 0x3333333333333333ull);
		c = (c & 0x0f0f0f0f0f0f0f0full) + ((c >> 4) & 0x0f0f0f0f0f0f0f0full);
		c = (c & 0x00ff00ff00ff00ffull) + ((c >> 8) & 0x00ff00ff00ff00ffull);
		c = (c & 0x0000ffff0000ffffull) + ((c >> 16) & 0x0000ffff0000ffffull);
		c = (c & 0x00000000ffffffffull) + ((c >> 32) & 0x00000000ffffffffull);	
		c = c;
	}

	{
		ivec2 map_index_origin(1);
		int hierarchy_ = 4;
		vec2 pos(49.7, 54.6);
		ivec2 map_index(pos);
		vec2 dir = normalize(vec2(100, 1));
//		vec2 inv_dir = normalize(abs(vec2(1.) / dir));
		dir *= abs(dir.x) >= abs(dir.y) ? abs(1. / dir.x) : abs(1. / dir.y);
		vec2 inv_dir = abs(vec2(1.) / dir);

		ivec2 cell_origin = map_index_origin << 3;
		ivec2 map_index_sub = map_index % 8;
		vec2 cell_p = abs(vec2(cell_origin) - (vec2(map_index_sub) + glm::fract(pos))) + vec2(0.5);
		vec2 axis = abs(cell_p*inv_dir);
		vec2 move = glm::max<float>(axis.x, axis.y)*dir;
//		ivec2 imove = (ivec2(move));
		ivec2 imove(cell_p);
		ivec2 r_begin = map_index % 8;
		ivec2 r_end = r_begin + imove;
		ivec2 area_begin = min(r_begin, r_end).xy;
		ivec2 area_end = max(r_begin, r_end).xy;
		ivec2 area = area_end - area_begin;
		area = clamp(area, 0, 7)+1;

		uint64_t x_line_mask = ((0x1ull << (area.x)) - 1) << area_begin.x;
		uint64_t x_mask = x_line_mask | (x_line_mask << 8) | (x_line_mask << 16) | (x_line_mask << 24) | (x_line_mask << 32) | (x_line_mask << 40) | (x_line_mask << 48) | (x_line_mask << 56);
		uint64_t y_mask = 0x1ull << (area.y*8);
		y_mask -= 1;
		y_mask <<= (area_begin.y * 8);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				uint64_t map = (1ull << (y * 8 + x));
				if ((x_mask&y_mask&map) != 0)
					printf("@");
				else
					printf("_");
			}
			printf("\n");
		}
		printf("---\n");

		int a = 0;

	}
	{
		int x_shift = 2;
		int y_shift = 6;
		int x_m = 2;
		int y_m = 1;

		uint64_t x_line_mask = ((0x1ull<<x_m)-1) << x_shift;
		uint64_t x_mask = x_line_mask | (x_line_mask << 8) | (x_line_mask << 16) | (x_line_mask << 24) | (x_line_mask << 32) | (x_line_mask << 40) | (x_line_mask << 48) | (x_line_mask << 56);
		uint64_t y_mask = ((0x1ull<<(y_m*8))-1) << (y_shift * 8);
		{
			auto c1 = x_mask & y_mask;
			for (int y = 0; y < 8; y++) {
				for (int x = 0; x < 8; x++) {
					uint64_t map = (1ull << (y * 8 + x));
					if ((c1&map) != 0)
						printf("@");
					else
						printf("_");
				}
				printf("\n");
			}
			printf("---\n");
		}
	}
	int x_shift = 3;
	int y_shift = 2;

	uint64_t x_line_mask = 0xffull & ~((1 << (8 - x_shift)) - 1);
	uint64_t x_mask_inv = x_line_mask | (x_line_mask << 8) | (x_line_mask << 16) | (x_line_mask << 24) | (x_line_mask << 32) | (x_line_mask << 40) | (x_line_mask << 48) | (x_line_mask << 56);
	uint64_t y_mask_inv = 0xffffffffffffffffull & ~((1ull << ((8 - y_shift) * 8)) - 1);
	auto x_mask = ~x_mask_inv;
	auto y_mask = ~y_mask_inv;
	{
		auto c1 = x_mask & y_mask;
		auto c2 = x_mask & (~y_mask);
		auto c3 = (~x_mask) & y_mask;
		auto c4 = (~x_mask) & (~y_mask);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				uint64_t map = (1ull << (y * 8 + x));
				if ((c1&map) != 0)
					printf("1");
				if ((c2&map) != 0)
					printf("2");
				if ((c3&map) != 0)
					printf("3");
				if ((c4&map) != 0)
					printf("4");
			}
			printf("\n");
		}
		printf("---\n");
	}

	{
		uint64_t x_line_mask2 = 0xffull & ((1 << (x_shift)) - 1);
		uint64_t x_mask_inv2 = x_line_mask2 | (x_line_mask2 << 8) | (x_line_mask2 << 16) | (x_line_mask2 << 24) | (x_line_mask2 << 32) | (x_line_mask2 << 40) | (x_line_mask2 << 48) | (x_line_mask2 << 56);
		uint64_t y_mask_inv2 = 0xffffffffffffffffllu & ((1ull << ((y_shift) * 8)) - 1);
		auto x_mask2 = ~x_mask_inv2;
		auto y_mask2 = ~y_mask_inv2;

		uint64_t map0 = 0xFFFFFFFFFFFFFFFFull;
		auto m1 = map0;
		auto m2 = map0;
		auto m3 = map0;
		auto m4 = map0;

		m1 &= x_mask2 & y_mask2;
		m2 &= x_mask2 & ~y_mask2;
		m3 &= ~x_mask2 & y_mask2;
		m4 &= ~x_mask2 & ~y_mask2;

		m1 >>= y_shift * 8;
		m1 >>= x_shift;

		m2 <<= (8 - y_shift) * 8;
		m2 >>= x_shift;

		m3 >>= y_shift * 8;
		m3 <<= (8 - x_shift);

		m4 <<= (8 - y_shift) * 8;
		m4 <<= (8 - x_shift);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				uint64_t map = (1ull << (y * 8 + x));
				if ((m1&map) != 0)
					printf("1");
				else if ((m2&map) != 0)
					printf("2");
				else if ((m3&map) != 0)
					printf("3");
				else if ((m4&map) != 0)
					printf("4");
			}
			printf("\n");
		}
		printf("---\n");

	}
	{
		uint64_t x_line_mask2 = 0xffull & ((1 << (x_shift)) - 1);
		uint64_t x_mask_inv2 = x_line_mask2 | (x_line_mask2 << 8) | (x_line_mask2 << 16) | (x_line_mask2 << 24) | (x_line_mask2 << 32) | (x_line_mask2 << 40) | (x_line_mask2 << 48) | (x_line_mask2 << 56);
		uint64_t y_mask_inv2 = 0xffffffffffffffffllu & ((1ull << ((y_shift) * 8)) - 1);
		auto x_mask2 = ~x_mask_inv2;
		auto y_mask2 = ~y_mask_inv2;

		uint64_t map0 = 0xFFFFFFFFFFFFFFFFull;
		auto m1 = map0;
		auto m2 = map0;
		auto m3 = map0;
		auto m4 = map0;

		m1 &= x_mask2 & y_mask2;
		m2 &= x_mask2 & ~y_mask2;
		m3 &= ~x_mask2 & y_mask2;
		m4 &= ~x_mask2 & ~y_mask2;

		m1 >>= y_shift * 8;
		m1 >>= x_shift;

		m2 <<= (8 - y_shift) * 8;
		m2 >>= x_shift;

		m3 >>= y_shift * 8;
		m3 <<= (8 - x_shift);

		m4 <<= (8 - y_shift) * 8;
		m4 <<= (8 - x_shift);

		m1 <<= y_shift * 8;
		m1 <<= x_shift;

		m2 >>= (8 - y_shift) * 8;
		m2 <<= x_shift;

		m3 <<= y_shift * 8;
		m3 >>= (8 - x_shift);

		m4 >>= (8 - y_shift) * 8;
		m4 >>= (8 - x_shift);
		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				uint64_t map = (1ull << (y * 8 + x));
				if ((m1&map) != 0)
					printf("1");
				if ((m2&map) != 0)
					printf("2");
				if ((m3&map) != 0)
					printf("3");
				if ((m4&map) != 0)
					printf("4");
			}
			printf("\n");
		}
		printf("---\n");

	}
	printf("---\n");

}

vec2 intersectRayRay(const vec2& as, const vec2& ad, const vec2& bs, const vec2& bd)
{
	float u = (as.y*bd.x + bd.y*bs.x - bs.y*bd.x - bd.y*as.x) / (ad.x*bd.y - ad.y*bd.x);
	if (u < 0.f)
	{
		return vec2(999999999.f);
	}
	return as + u * ad;
}

bool marchToAABB(vec2& p, vec2 d, vec2 bmin, vec2 bmax)
{
 
 	if (all(lessThan(p, bmax))
 		&& all(greaterThan(p, bmin)))
 	{
 		// AABBの中にいる
 		return true;
 	}

	float tmin = 0.;
	float tmax = 10e6;
	for (int i = 0; i < 2; i++)
	{
		if (abs(d[i]) < 10e-6)
		{
			// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
			if (p[i] < bmin[i] || p[i] > bmax[i])
			{
				p = vec2(-9999999999.f);
				return false;
			}
		}
		else
		{
			float ood = 1. / d[i];
			float t1 = (bmin[i] - p[i]) * ood;
			float t2 = (bmax[i] - p[i]) * ood;

			// t1が近い平面との交差、t2が遠い平面との交差になる
			float near_ = glm::min(t1, t2);
			float far_ = glm::max(t1, t2);

			// スラブの交差している感覚との交差を計算
			tmin = glm::max(near_, tmin);
			tmax = glm::max(far_, tmax);

			if (tmin > tmax) {
				p = vec2(-9999999999.f);
				return false;
			}
		}
	}
	float dist = tmin;
	p += d * dist;
	return true;

}

bool intersectRayAABB(vec2 p, vec2 d, vec2 bmin, vec2 bmax, float& d_min, float& d_max)
{

	vec2 tmin = vec2(0.f);
	vec2 tmax = vec2(0.f);
	for (int i = 0; i < 2; i++)
	{
		if (abs(d[i]) < 10e-6)
		{
			// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
			if (p[i] < bmin[i] || p[i] > bmax[i])
			{
				p = vec2(-9999999999.f);
				return false;
			}
			tmin[i] = 0.f;
			tmax[i] = 99999.f;
		}
		else
		{
			float ood = 1. / d[i];
			float t1 = (bmin[i] - p[i]) * ood;
			float t2 = (bmax[i] - p[i]) * ood;

			tmin[i] = glm::min(t1, t2);
			tmax[i] = glm::max(t1, t2);
		}
	}
	d_min = glm::max(tmin[0], tmin[1]);
	d_max = glm::min(tmax[0], tmax[1]);
	return d_min <= d_max;
}

int getMortonIndex(ivec2 xy)
{
	// 8x8のブロック
	ivec2 n = xy >> 3;
	xy -= n<<3;
	n = (n ^ (n << 8)) & 0x00ff00ff;
	n = (n ^ (n << 4)) & 0x0f0f0f0f;
	n = (n ^ (n << 2)) & 0x33333333;
	n = (n ^ (n << 1)) & 0x55555555;

	int mi = (n.y << 1) | n.x;
	return mi * 64 + xy.x + xy.y * 8;
}

#define denominator (128.f)
uint packEmissive(vec3 rgb)
{
	ivec3 irgb = ivec3(rgb*denominator*(1.f+1.f/denominator*0.5f));
	irgb <<= ivec3(20, 10, 0);
	return irgb.x | irgb.y | irgb.z;
}
vec3 unpackEmissive(uint irgb)
{
	vec3 rgb = vec3((ivec3(irgb) >> ivec3(20, 10, 0)) & ((1<<ivec3(10, 10, 10))-1));
	return rgb / denominator;
}

void bitonic_sort()
{
	std::vector<int> buffer(1024<<2);
	for (auto& b : buffer)
	{
		b = std::rand() % 10000;
	}

	for (int step = 2; step <= buffer.size(); step <<= 1) {
		for (int offset = step >> 1; offset > 0; offset = offset >> 1) {

			for (int i = 0; i < buffer.size(); i++)
			{
				uint index_a = i;
				uint index_b = index_a ^ offset;

				if (index_a >= buffer.size() || index_b >= buffer.size())
				{
					continue;
				}
				// 自分より上のものだけチェックする
				if (index_a < index_b)
				{
					continue;
				}

				auto a = buffer[index_a];
				auto b = buffer[index_b];
				if (((index_a&step) == 0) == a<b)
				{
					buffer[index_a] = b;
					buffer[index_b] = a;
				}
			}
		}
	}

	for (auto& b : buffer)
	{
		printf("%6d ", b);
	}
	printf("\n");
}

vec2 rotateslerp(float angle)
{
	float a = angle / glm::two_pi<float>() * 4.f;
	float t = fmod(a, 1.f);
	int index = (int)a;
	vec3 pp[] = {
		vec3(1., 0., 0.),
		vec3(0., 1., 0.),
		vec3(-1.,0., 0.),
		vec3(0.,-1., 0.),
	};
	vec3 p0 = pp[index];
	vec3 p1 = pp[(index + 1) % 4];
	auto p = glm::slerp(p0, p1, t);
	return p.xy();
}
vec2 rotatelerp(float angle)
{
	float a = angle / glm::two_pi<float>() * 4.f;
	float t = fmod(a, 1.f);
	int index = (int)a;
	vec3 pp[] = {
		vec3(1., 0., 0.),
		vec3(0., 1., 0.),
		vec3(-1.,0., 0.),
		vec3(0.,-1., 0.),
	};
	vec3 p0 = pp[index];
	vec3 p1 = pp[(index + 1) % 4];
	auto p = glm::lerp(p0, p1, t);
	return glm::normalize(p.xy());
}

void raymarch()
{
	vec2 pos(123.f, 145.f);
	vec2 dir = normalize(vec2(-0.7f, -2.f));
	vec2 inv_dir;
	inv_dir.x = dir.x == 0. ? 999999. : abs(1. / dir.x);
	inv_dir.y = dir.y == 0. ? 999999. : abs(1. / dir.y);

	ivec2 map_index = ivec2(pos);
	int hierarchy = 1;
	ivec2 cell_origin = ivec2(greaterThanEqual(dir, vec2(0.))) << hierarchy;
	ivec2 map_index_sub = map_index - ((map_index >> hierarchy) << hierarchy);
	vec2 tp = abs(vec2(cell_origin) - (vec2(map_index_sub) + fract(pos))) * inv_dir;
	vec2 delta = abs((1 << hierarchy)*inv_dir);

	for (int _i = 0; _i < 50; _i++)
	{
		int axis = int(tp.x > tp.y);
		tp[axis] += delta[axis];
		map_index[axis] += ivec2(greaterThanEqual(dir, vec2(0.)))[axis] * 2 - 1;

		printf("[%5d,%5d]\n", map_index.x, map_index.y);
	}
}

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

layout(local_size_x=64) in;
layout(set=0, binding=0, buffer_reference, scalar) buffer Texcoord {vec2 b_t[]; };
void main()
{

}
)";
std::vector<uint32_t> read_file(const std::string& filename)
{
	std::filesystem::path filepath(filename);
	std::ifstream file(filepath, std::ios_base::ate | std::ios::binary);
	if (!file.is_open()) {
		printf("file not found \"%s\"", filename.c_str());
		assert(file.is_open());
	}

	size_t file_size = (size_t)file.tellg();
	file.seekg(0);

	std::vector<uint32_t> buffer(file_size/4);
	file.read((char*)buffer.data(), file_size);

	return buffer;
}

std::vector<uint32_t> comileGlslToSpirv()
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
	Resources.maxComputeWorkGroupSizeX = 64;
	Resources.maxComputeWorkGroupSizeY = 1;
	Resources.maxComputeWorkGroupSizeZ = 1;
	if (!shader->parse(&Resources, 100, false, messages)) {
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
	//	spirv_binary = read_file(btr::getResourceShaderPath() + "SN_Render.vert.spv");
	spirv_cross::CompilerGLSL glsl(spirv_binary);

	// The SPIR-V is now parsed, and we can perform reflection on it.
	spirv_cross::ShaderResources resources = glsl.get_shader_resources();

	// Get all sampled images in the shader.
	for (auto& resource : resources.sampled_images)
	{
		unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
		unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

		// Modify the decoration to prepare it for GLSL.
		glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);

		// Some arbitrary remapping if we want.
		glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
	}
	for (auto& resource : resources.uniform_buffers)
	{
		unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
		unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

		// Modify the decoration to prepare it for GLSL.
		glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);

		// Some arbitrary remapping if we want.
		glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
	}

	for (auto& resource : resources.storage_buffers)
	{
		unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
		unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
		printf("storage %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);

		// Modify the decoration to prepare it for GLSL.
		glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);

		// Some arbitrary remapping if we want.
		glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
	}

	// Set some options.
	spirv_cross::CompilerGLSL::Options options;
	options.version = 450;
	glsl.set_common_options(options);

	// Compile to GLSL, ready to give to GL driver.
	std::string source = glsl.compile();
	return spirv_binary;
}

int main()
{
	comileGlslToSpirv();
// 	{
// 		// Read SPIR-V from disk or similar.
// 		std::vector<uint32_t> spirv_binary = load_spirv_file();
// 
// 		spirv_cross::CompilerGLSL glsl(std::move(spirv_binary));
// 
// 		// The SPIR-V is now parsed, and we can perform reflection on it.
// 		spirv_cross::ShaderResources resources = glsl.get_shader_resources();
// 
// 		// Get all sampled images in the shader.
// 		for (auto& resource : resources.sampled_images)
// 		{
// 			unsigned set = glsl.get_decoration(resource.id, spv::DecorationDescriptorSet);
// 			unsigned binding = glsl.get_decoration(resource.id, spv::DecorationBinding);
// 			printf("Image %s at set = %u, binding = %u\n", resource.name.c_str(), set, binding);
// 
// 			// Modify the decoration to prepare it for GLSL.
// 			glsl.unset_decoration(resource.id, spv::DecorationDescriptorSet);
// 
// 			// Some arbitrary remapping if we want.
// 			glsl.set_decoration(resource.id, spv::DecorationBinding, set * 16 + binding);
// 		}
// 
// 		// Set some options.
// 		spirv_cross::CompilerGLSL::Options options;
// 		options.version = 310;
// 		options.es = true;
// 		glsl.set_common_options(options);
// 
// 		// Compile to GLSL, ready to give to GL driver.
// 		std::string source = glsl.compile();
// 	}


	if(0)
	{
		auto origin = vec2(7.99f, 8.99f);
		auto pos = origin;
		auto dir = normalize(vec2(2.0247f, 7.1f));
		vec2 inv_dir;
		inv_dir.x = dir.x == 0. ? 999999. : abs(1. / dir.x);
		inv_dir.y = dir.y == 0. ? 999999. : abs(1. / dir.y);
		dir *= glm::min(inv_dir.x, inv_dir.y);
		int axis = inv_dir.x < inv_dir.y ? 0 : 1;
		int march_count = 0;
		for (int i = 0; i < 100; i++)
		{
			ivec2 map_index = ivec2(pos);
			ivec2 cell = map_index >> 3;
			ivec2 cell_sub = map_index % 8;

			vec2 cell_origin = vec2(greaterThanEqual(dir, vec2(0.))) * vec2(8);
			vec2 pos_sub = vec2(pos - vec2(cell << 3));
			vec2 tp = vec2(abs(cell_origin - pos_sub)) / abs(dir);
			int _axis = tp.x < tp.y ? 0 : 1;
			float skip = tp[_axis];
			march_count += (axis == _axis) ? glm::max(skip, 1.f) : glm::max(skip, 1.f);
			pos = origin + dir * march_count;
			map_index = ivec2(pos);

			printf("%3d =[%f, %f] [%5d, %5d]\n", i, pos.x, pos.y, map_index.x, map_index.y);

//			vec2 tp = 0.5*(1 << hierarchy)*(1 << u_gi2d_scene.m_skip)*inv_dir;
	//		//	vec2 tp = (vec2(u_gi2d_scene.m_frame%2, u_gi2d_scene.m_frame/2) *0.5 + 0.25)*inv_dir;
		//	vec3 delta = vec3(inv_dir, 0.) * (1 << hierarchy) * (1 << u_gi2d_scene.m_skip);
			//ivec3 next = ivec3(ivec2(greaterThanEqual(dir, vec2(0.))) * 2 - 1, 0)* (1 << hierarchy) * (1 << u_gi2d_scene.m_skip);

		}
	}
	{
		for (int i = 0; i < 256; i++)
		{
			auto dir = glm::rotate(glm::vec2(1.f, 0.f), (6.28f / 256.f) * (i+0.5f));
			vec2 inv_dir = abs(1.f / dir);
			vec2 segment_one = dir * glm::max(inv_dir.x, inv_dir.y);
			dir *= glm::min(inv_dir.x, inv_dir.y);
			ivec2 idir = ivec2(dir);
			printf("%3d =[%f, %f]\n", i, dir.x, dir.y);
			printf(" segment_one=[%f, %f]\n", segment_one.x, segment_one.y);
		}

		int a = 0;
		a++;
	}

	raymarch();

	{
		int hierarchy = 5;
		uint buf_offset = 0;
		ivec2 reso(256);
		uint radiance_offset = reso.x*reso.y;
		for (int i = 0; i < hierarchy; i++) {
			buf_offset += reso.x*reso.y >> (2 * i);
			radiance_offset >>= 2;
		}
		hierarchy++;

	}
	{
		int loop = 360;
		for (size_t i = 0; i < loop; i++)
		{
//			vec2 sdir = rotateslerp(glm::two_pi<float>() *  i / loop);
			vec2 dir = glm::rotate(vec2(1.f, 0.f), glm::two_pi<float>() *  i / loop);
			vec2 ldir = rotatelerp(glm::two_pi<float>() *  i / loop);
//			printf("[%3d] [x,y]=[%7.5f,%7.5f]=[%7.5f,%7.5f]\n", i, sdir.x, sdir.y, dir.x, dir.y);
//			printf("[%3d] [x,y]=[%7.5f,%7.5f]=[%7.5f,%7.5f]\n", i, ldir.x, ldir.y, dir.x, dir.y);
		}
		int iyyi = 0;
		iyyi++;

	}
	{
//		bitonic_sort();
	}
	{
//		auto a = getMortonIndex(ivec2(123, 256));
//		a++;
	}
	{
		for (int i = 0; i < 100; i++)
		{
			auto o = vec3((rand() % 255) / 255.f, (rand() % 255) / 255.f, (rand() % 255) / 255.f);
			auto a = packEmissive(o);
			auto a3 = unpackEmissive(a);

//			printf("original=[%8.5f,%8.5f,%8.5f] test=[%8.5f,%8.5f,%8.5f]\n", o.x, o.y, o.z, a3.x,a3.y,a3.z);
		}
		float f11_max = (1 << 11) / denominator;
		float f10_max = (1 << 10) / denominator;
//		printf("f10=%8.5f, f11=%8.5f\n", f10_max, f11_max);

		{
			auto a = packEmissive(vec3(0.1f, 0.01f, 0.001f));
			auto a3 = unpackEmissive(a);

//			printf("test=[%8.5f,%8.5f,%8.5f]\n", a3.x, a3.y, a3.z);

		}
	}
	{
#define gl_LocalInvocationIndex (3)
		int loop = 64;
		for (int i = 0; i < loop; i++) 
		{
			vec2 dir = glm::rotate(vec2(1.f, 0.f), i*6.28f / loop);
			vec2 begin;
			vec2 end;
			{
				vec2 floorp;
				floorp.x = dir.x >= 0 ? 0.f : 512.f;
				floorp.y = dir.y >= 0 ? 0.f : 512.f;
				vec2 floordir;
				floordir.x = abs(dir.x) > abs(dir.y) ? 0.f : 1.f;
				floordir.y = abs(dir.x) > abs(dir.y) ? 1.f : 0.f;

				auto p0 = intersectRayRay(vec2(0, 0), dir, floorp, floordir);
				auto p1 = intersectRayRay(vec2(512, 0), dir, floorp, floordir);
				auto p2 = intersectRayRay(vec2(0, 512), dir, floorp, floordir);
				auto p3 = intersectRayRay(vec2(512, 512), dir, floorp, floordir);

				vec2 minp = glm::min(glm::min(glm::min(p0, p1), p2), p3);
				begin = minp + floordir * (8.) * gl_LocalInvocationIndex;
				float _min, _max;
// 				if (intersectRayAABB(begin, dir, vec2(0.f), vec2(512.f), _min, _max))
// 				{
// 					end = begin + dir * _max;
// 					begin = begin + dir * _min;
// 				}
// 				else {
// 					begin = end = vec2(99999.f);
// 				}
				if (marchToAABB(begin, dir, vec2(0.), vec2(512.f)))
				{
					end = vec2(0.f);
				}
				else
				{
					begin = end = vec2(99999.f);
				}

			}
			printf("min=[%6.1f,%6.1f] max=[%6.1f,%6.1f] dir=[%3.1f,%3.1f]\n", begin.x, begin.y, end.x, end.y, dir.x, dir.y);
			printf("  length=[%6.2f]\n", distance(begin, end));

			vec2 side = glm::rotate(dir, -3.14f*0.5f);
			side.x = abs(side.x) < 0.0001 ? 0.0001 : side.x;
			side.y = abs(side.y) < 0.0001 ? 0.0001 : side.y;
			vec2 invside = abs(1.f / side);
			vec2 side1 = side * glm::min(invside.x, invside.y);

			vec2 origin;
			origin.x = side.x > 0.f ? 0.f : 7.f;
			origin.y = side.y > 0.f ? 0.f : 7.f;
			ivec4 mask_x = ivec4(side1.xxxx * vec4(0.5, 2.5, 4.5, 6.5) + origin.xxxx);
			ivec4 mask_y = ivec4(side1.yyyy * vec4(0.5, 2.5, 4.5, 6.5) + origin.yyyy);
			auto hit_mask = glm::u64vec4(1ul) << glm::u64vec4(mask_x + mask_y * 8);

//			printf("[%2d] dir=[%5.2f,%5.2f] side=[%5.2f,%5.2f], mask=[%3d%3d%3d%3d] \n", i, dir.x, dir.y, side.x, side.y, hit_mask.x, hit_mask.y, hit_mask.z, hit_mask.w);

		}
		vec2 dir = normalize(vec2(0.1f, 1.f));
		dir.x = abs(dir.x) < 0.0001 ? 0.0001 : dir.x;
		dir.y = abs(dir.y) < 0.0001 ? 0.0001 : dir.y;

		vec2 floorp;
		floorp.x = dir.x >= 0 ? 0.f : 512.f;
		floorp.y = dir.y >= 0 ? 0.f : 512.f;
		vec2 floordir;
		floordir.x = abs(dir.x) > abs(dir.y) ? 0.f : 1.f;
		floordir.y = abs(dir.x) > abs(dir.y) ? 1.f : 0.f;

		auto p = intersectRayRay(vec2(0, 512), dir, floorp, floordir);

		vec2 side = glm::rotate(dir, -3.14f*0.5f);
		side.x = abs(side.x) < 0.0001 ? 0.0001 : side.x;
		side.y = abs(side.y) < 0.0001 ? 0.0001 : side.y;
		vec2 invside = abs(1.f / side);
		side *= glm::min(invside.x, invside.y);

		ivec2 origin;
		origin.x = side.x > 0. ? 0 : 7;
		origin.y = side.y > 0. ? 0 : 7;
		auto s0 = (ivec2)(side * (0.5f + 0.f))+origin;
		auto s1 = (ivec2)(side * (0.5f + 2.f))+origin;
		auto s2 = (ivec2)(side * (0.5f + 4.f))+origin;
		auto s3 = (ivec2)(side * (0.5f + 6.f))+origin;
		uint64_t bit = 0;
		bit |= (1ull << (s0.x + s0.y * 8));
		bit |= (1ull << (s1.x + s1.y * 8));
		bit |= (1ull << (s2.x + s2.y * 8));
		bit |= (1ull << (s3.x + s3.y * 8));

		for (int y = 0; y < 8; y++) {
			for (int x = 0; x < 8; x++) {
				uint64_t map = (1ull << (y * 8 + x));
				if ((bit&map) != 0)
					printf("@");
				else
					printf("_");
			}
			printf("\n");
		}
		printf("---\n");

	}

	std::array<uvec4, 16 * 16> s_alive_num4;
	std::array<uint, 32 * 32>  s_alive_num;
	std::array<uvec4, 16 * 16> b_alive_num;
	for (int y = 0; y < 32; y++)
	{
		for (int x = 0; x < 32; x++)
		{
			int ii = y / 2 * 16 + x / 2;
			uvec2 i = uvec2(x, y);
			uvec2 m = i % uvec2(2);
			uint index = (ii % 32);
			uint offset = ii / 32 * 32;
			//	s_alive_num[offset + n.x + n.y*16][m.x + m.y*2] = uint(popcnt(is_alive));
			s_alive_num4[offset + index][m.x + m.y * 2] = x + y * 32;
		}
	}
	for (int y = 0; y < 32; y++)
	{
		for (int x = 0; x < 32; x++)
		{
			int ii = y / 2 * 16 + x / 2;
			uvec2 i = uvec2(x, y);
			uvec2 m = i % uvec2(2);
			uint index = (ii % 32);
			uint offset = ii / 32 * 32;
			//	s_alive_num[offset + n.x + n.y*16][m.x + m.y*2] = uint(popcnt(is_alive));
			s_alive_num[(offset + index)*4 + m.x + m.y * 2] = x + y * 32;
		}
	}

	for (int y = 0; y < 32; y++)
	{
		for (int x = 0; x < 32; x++)
		{
			int ii = y / 2 * 16 + x / 2;
			uvec2 i = uvec2(x, y);
			uvec2 m = i % uvec2(2);
			uint index = (ii % 32);
			uint offset = ii / 32 * 32;

			if (all(equal(m, uvec2(0))))
			{
				uvec4 c = s_alive_num4[ii];
				c <<= uvec4(0, 8, 16, 24);
				uint count = c.x | c.y | c.z | c.w;

				uint rt_map_size = 1024;
				uint rt_tile_index = x / 2 + (y / 2)*16;
				b_alive_num[rt_tile_index] = c;
			}
		}
	}



	bittest();
	uint32_t bit = 0;
	btr::setBit(bit, 1257, 16, 16);
	btr::setBit(bit, 1145, 0, 16);
	auto value = btr::getBit(bit, 0, 16);
	auto valuehi = btr::getBit(bit, 16, 16);

	glm::vec3 a(0.f, 0.f, 1.f);
	glm::vec3 b = glm::normalize(glm::vec3(1.f, 0.f, 1.f));
	auto angle = glm::degrees(glm::asin(glm::cross(a, b).y));
	auto mask = glm::u64vec3((1ull << 22ull) - 1, (1ull << 42ull) - 1, std::numeric_limits<uint64_t>::max());
	mask.z -= mask.y;
	mask.y -= mask.x;

	btr::setResourceAppPath("..\\..\\resource\\000_api_test\\");


	return 0;
}

