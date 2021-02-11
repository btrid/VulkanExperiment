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
#include <applib/sCameraManager.h>

#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

struct Voxel
{
	enum
	{
		DSL_Voxel,
		DSL_Num,
	};
	enum
	{
		DS_Voxel,
		DS_Num,
	};

	enum
	{
		Pipeline_MakeVoxelBottom,
		Pipeline_MakeVoxelTop,
		Pipeline_RenderVoxel,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_MakeVoxel,
		PipelineLayout_RenderVoxel,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	struct VoxelInfo
	{
		uvec4 reso;
		uvec4 bottom_brick;
		uvec4 top_brick;
		uvec4 bottom_reso;
		uvec4 top_reso;
		uint material_size;
	};
	struct Material
	{
		uint a;
	};
	btr::BufferMemoryEx<VoxelInfo> u_info;
	btr::BufferMemoryEx<uvec4> b_map_bottom;
	btr::BufferMemoryEx<uvec4> b_map_top;
	btr::BufferMemoryEx<uvec4> b_material_counter;
	btr::BufferMemoryEx<uint> b_material_id;
	btr::BufferMemoryEx<uint> b_material;

	VoxelInfo m_info;

	Voxel(btr::Context& ctx)
	{
		auto cmd = ctx.m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL[DSL_Voxel] = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}

		// descriptor set
		{
			m_info.reso = uvec4(2048, 2048, 512, 1);
			m_info.bottom_brick = uvec4(4, 4, 8, 1);
			m_info.top_brick = uvec4(4, 4, 8, 1);
			m_info.bottom_reso = m_info.reso / m_info.bottom_brick;
			m_info.top_reso = m_info.bottom_reso / m_info.top_brick;
			m_info.material_size = 20000;

			u_info = ctx.m_uniform_memory.allocateMemory<VoxelInfo>(1);
			b_map_top = ctx.m_storage_memory.allocateMemory<uvec4>(m_info.top_reso.x * m_info.top_reso.y * m_info.top_reso.z, {});
			b_map_bottom = ctx.m_storage_memory.allocateMemory<uvec4>(m_info.bottom_reso.x * m_info.bottom_reso.y * m_info.bottom_reso.z, {});
			b_material_counter = ctx.m_storage_memory.allocateMemory<uvec4>(1);
			b_material_id = ctx.m_storage_memory.allocateMemory<uint>(1);
			b_material = ctx.m_storage_memory.allocateMemory<uint>(m_info.material_size);

			cmd.updateBuffer<VoxelInfo>(u_info.getInfo().buffer, u_info.getInfo().offset, m_info);


			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[DSL_Voxel].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS[DS_Voxel] = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] =
			{
				u_info.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] =
			{
				b_map_top.getInfo(),
				b_map_bottom.getInfo(),
				b_material_counter.getInfo(),
				b_material_id.getInfo(),
				b_material.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(m_DS[DS_Voxel].get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(storages))
				.setPBufferInfo(storages)
				.setDstBinding(1)
				.setDstSet(m_DS[DS_Voxel].get()),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DSL_Voxel].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_MakeVoxel] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}
			{
				vk::DescriptorSetLayout layouts[] =
				{
					m_DSL[DSL_Voxel].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_RenderVoxel] = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Voxel_MakeBottom.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_MakeTop.comp.spv", vk::ShaderStageFlagBits::eCompute},
				{"Voxel_Rendering.comp.spv", vk::ShaderStageFlagBits::eCompute},
			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			vk::ComputePipelineCreateInfo compute_pipeline_info[] =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[0])
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[1])
				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shaderStages[2])
				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
			};
			m_pipeline[Pipeline_MakeVoxelBottom] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
			m_pipeline[Pipeline_MakeVoxelTop] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
			m_pipeline[Pipeline_RenderVoxel] = ctx.m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
		}
	}

	void execute_MakeVoxel(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, __FUNCTION__);

		_label.insert("Make Voxel Bottom");
		{
			{
				// 			vk::BufferMemoryBarrier barrier[] =
				// 			{
				// 				base.b_ldc_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				// 				base.b_ldc_point_link_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				// 				base.b_ldc_point.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				// 				boolean.m_TLAS.m_AS_buffer.makeMemoryBarrier(vk::AccessFlagBits::eAccelerationStructureWriteKHR, vk::AccessFlagBits::eShaderRead)
				// 			};
				// 			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});

			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelBottom].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

			auto num = m_info.bottom_reso.xyz();
			cmd.dispatch(num.x, num.y, num.z);

		}

		_label.insert("Make Voxel Top");
		{
			{
				vk::BufferMemoryBarrier barrier[] =
				{
					b_map_bottom.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_size(barrier), barrier }, {});
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeVoxelTop].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_MakeVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});

			auto num = m_info.top_reso.xyz();
			cmd.dispatch(num.x, num.y, num.z);

		}
	}
	void execute_RenderVoxel(vk::CommandBuffer cmd, RenderTarget& rt)
	{
		{
			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(rt.m_image);
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier.setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader,
				{}, {}, { /*array_size(to_read), to_read*/ }, { image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RenderVoxel].get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 0, { m_DS[DSL_Voxel].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 1, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_PL[PipelineLayout_RenderVoxel].get(), 2, { rt.m_descriptor.get() }, {});

		auto num = app::calcDipatchGroups(uvec3(1024, 1024, 1), uvec3(8, 8, 1));
		cmd.dispatch(num.x, num.y, num.z);
	}
};

bool intersection(vec3 aabb_min, vec3 aabb_max, vec3 pos, vec3 inv_dir, float& n, float& f)
{
	// https://tavianator.com/fast-branchless-raybounding-box-intersections/
	vec3 t1 = (aabb_min - pos) * inv_dir;
	vec3 t2 = (aabb_max - pos) * inv_dir;

	vec3 tmin = min(t1, t2);
	vec3 tmax = max(t1, t2);

	n = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
	f = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

	return f > n;
}

void dda()
{
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.42.3443&rep=rep1&type=pdf
	vec3 p = vec3(125, 23, 28);
	vec3 d = normalize(vec3(-40, -7, -88));
	vec3 inv_d;
	inv_d.x = abs(d.x) < 0.000001 ? 999999.f : 1. / d.x;
	inv_d.y = abs(d.y) < 0.000001 ? 999999.f : 1. / d.y;
	inv_d.z = abs(d.z) < 0.000001 ? 999999.f : 1. / d.z;

	float n, f;
	intersection(vec3(0), vec3(512), p, inv_d, n, f);

//	vec3 target = p + d * f;
//	vec3 delta = target - p;
//	vec3 delta_error = abs(inv_d);

#if 1
	int axis = abs(d).x > abs(d).y ? 0 : 1;
	axis = abs(d)[axis] > abs(d).z ? axis : 2;
	d *= abs(inv_d)[axis];
	inv_d /= abs(inv_d)[axis];
	vec3 error = mix(fract(p), vec3(1.) - fract(p), step(d, vec3(0.)));
	while (all(lessThan(p, vec3(512.f))) && all(greaterThanEqual(p, vec3(0.f))))
	{
		printf("%6.2f,%6.2f,%6.2f\n", p.x, p.y, p.z);
		error += abs(d);
		p += glm::sign(d) * floor(error);
 		error = fract(error);

 		intersection(vec3(0), vec3(512), p, inv_d, n, f);
 		error += abs(d) * glm::max(f + 0.01f, 0.f);
 		p += glm::sign(d) * floor(error);
 		error = fract(error);

	}
// #elif 0
//	vec3 error = mix(vec3(1.) - glm::fract(p), glm::fract(p), step(d, vec3(0.))) * abs(inv_d);
// 	while (all(lessThan(p, vec3(512.f))) && all(greaterThanEqual(p, vec3(0.f))))
// 	{
// 		printf("%6.2f,%6.2f,%6.2f\n", p.x, p.y, p.z);
// 		p.x += glm::sign(d.x);
// 		error.y += abs(inv_d).y;
// 		while (error.y >= 0.5)
// 		{
// 			p.y += glm::sign(delta.y);
// 			error.y--;
// 		}
// 		error.z += deltaError.z;
// 		while (error.z >= 0.5)
// 		{
// 			p.z += glm::sign(delta.z);
// 			errorZ--;
// 		}
// 	}
// 
#elif 1
	vec3 error = mix(fract(p), vec3(1.) - fract(p), step(d, vec3(0.))) * abs(inv_d);
	while (all(lessThan(p, vec3(512.f))) && all(greaterThanEqual(p, vec3(0.f))))
	{
		printf("%6.2f,%6.2f,%6.2f\n", p.x, p.y, p.z);
		int axis = (error.x < error.y) ? 0 : 1;
		axis = error[axis] < error.z ? axis : 2;
		p[axis] += glm::sign(d[axis]);
		error[axis] += abs(inv_d[axis]);
	}
#endif
	int a = 0;
}
int main()
{
	dda();
	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-1000.f, 0.f, -500.f);
	camera->getData().m_target = vec3(0.f, 200.f, 0.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;


	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	Voxel voxel(*context);
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		voxel.execute_MakeVoxel(cmd);
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
					voxel.execute_RenderVoxel(cmd, *app.m_window->getFrontBuffer());
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

