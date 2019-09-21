#pragma once

#include <memory>
#include <btrlib/Define.h>
#include <btrlib/Context.h>

struct GI2DDescription
{
	int32_t RenderWidth;
	int32_t RenderHeight;
};
struct GI2DContext
{
	enum Layout
	{
		Layout_Data,
		Layout_SDF,
		Layout_Path,
		Layout_Num,
	};
	int32_t RenderWidth;
	int32_t RenderHeight;
	ivec2 RenderSize;
	int FragmentBufferSize;
	struct GI2DInfo
	{
		mat4 m_camera_PV;
		uvec4 m_resolution;
		vec4 m_position;
		std::array<uint32_t, 4> m_fragment_map_size_hierarchy;
		uint32_t m_hierarchy_num;

		uint32_t  getsize(int32_t hierarchy) {
			return m_fragment_map_size_hierarchy[hierarchy] - m_fragment_map_size_hierarchy[hierarchy-1];
		}
	};
	struct GI2DScene
	{
		int32_t m_frame;
		int32_t m_hierarchy;
		uint32_t m_skip;
		int32_t _p2;

		uint32_t m_radiance_offset;
		uint32_t m_map_offset;
		uvec2 m_map_reso;
	};

	struct Fragment
	{
		uint32_t data : 30;
		uint32_t is_diffuse : 1;
		uint32_t is_emissive : 1;
		Fragment()
		{}
		Fragment(const vec3& rgb, bool d, bool e)
		{
			data = packRGB(rgb);
			is_diffuse = d ? 1 : 0;
			is_emissive = e ? 1 : 0;
		}
#define _maxf (1023.f)
#define _maxi (1023)
		static uint packRGB(const vec3& rgb)
		{
			ivec3 irgb = ivec3(rgb*_maxf);
			irgb = glm::min(irgb, ivec3(_maxi));
			irgb <<= ivec3(20, 10, 0);
			return irgb.x | irgb.y | irgb.z;
		}
		static vec3 unpackRGB(uint irgb)
		{
			vec3 rgb = vec3((uvec3(irgb) >> uvec3(20, 10, 0)) & ((uvec3(1) << uvec3(10, 10, 10)) - uvec3(1)));
			return rgb / _maxf;
		}

		void setRGB(const vec3& rgb)
		{
			data = packRGB(rgb);
		}
		vec3 getRGB()const
		{
			return unpackRGB(data);
		}
	};

	struct LinkList
	{
		int32_t next;
		int32_t target;
	};

	GI2DContext(const std::shared_ptr<btr::Context>& context, const GI2DDescription& desc)
	{
 		RenderWidth = desc.RenderHeight;
 		RenderHeight = desc.RenderWidth;
		RenderSize = ivec2(RenderWidth, RenderHeight);
		FragmentBufferSize = RenderWidth * RenderHeight;

		m_context = context;

		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout[Layout_Data] = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout[Layout_SDF] = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);

			}
			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout[Layout_Path] = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);

			}
		}

		// descriptor
		{
			auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
			{
				u_gi2d_info = context->m_uniform_memory.allocateMemory<GI2DInfo>({ 1,{} });
				u_gi2d_scene = context->m_uniform_memory.allocateMemory<GI2DScene>({ 1,{} });

				m_gi2d_info.m_position = vec4(0.f, 0.f, 0.f, 0.f);
				m_gi2d_info.m_resolution = uvec4(RenderWidth, RenderHeight, RenderWidth / 8, RenderHeight / 8);
				m_gi2d_info.m_camera_PV = glm::ortho(RenderWidth*-0.5f, RenderWidth*0.5f, RenderHeight*-0.5f, RenderHeight*0.5f, 0.f, 2000.f) * glm::lookAt(vec3(RenderWidth*0.5f, 1000.f, RenderHeight*0.5f) + m_gi2d_info.m_position.xyz(), vec3(RenderWidth*0.5f, 0.f, RenderHeight*0.5f) + m_gi2d_info.m_position.xyz(), vec3(0.f, 0.f, 1.f));
				m_gi2d_info.m_hierarchy_num = 4;

				int size = RenderHeight * RenderWidth / 64;
				m_gi2d_info.m_fragment_map_size_hierarchy[0] = size;
				size /= 64;
				m_gi2d_info.m_fragment_map_size_hierarchy[1] = m_gi2d_info.m_fragment_map_size_hierarchy[0] + size;
				size /= 64;
				m_gi2d_info.m_fragment_map_size_hierarchy[2] = m_gi2d_info.m_fragment_map_size_hierarchy[1] + size;
				size /= 64;
				m_gi2d_info.m_fragment_map_size_hierarchy[3] = m_gi2d_info.m_fragment_map_size_hierarchy[2] + glm::max(size, 1);

				cmd.updateBuffer<GI2DInfo>(u_gi2d_info.getInfo().buffer, u_gi2d_info.getInfo().offset, m_gi2d_info);

				m_gi2d_scene.m_frame = 0;
				m_gi2d_scene.m_hierarchy = 0;
				m_gi2d_scene.m_skip = 0;
				cmd.updateBuffer<GI2DScene>(u_gi2d_scene.getInfo().buffer, u_gi2d_scene.getInfo().offset, m_gi2d_scene);

				{
					vk::BufferMemoryBarrier to_read[] = {
						u_gi2d_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
						u_gi2d_scene.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, {},
						0, nullptr, array_length(to_read), to_read, 0, nullptr);
				}

			}
			{
				b_fragment = context->m_storage_memory.allocateMemory<Fragment>({ FragmentBufferSize,{} });
				b_fragment_map = context->m_storage_memory.allocateMemory<uint64_t>({ m_gi2d_info.m_fragment_map_size_hierarchy[0]*2,{} });
				b_grid_counter = context->m_storage_memory.allocateMemory<int32_t>({ FragmentBufferSize,{} });
			}
		}

		// descriptor set
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout[Layout_Data].get(),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo uniforms[] = {
					u_gi2d_info.getInfo(),
					u_gi2d_scene.getInfo(),
				};
				vk::DescriptorBufferInfo storages[] = {
					b_fragment.getInfo(),
					b_fragment_map.getInfo(),
					b_grid_counter.getInfo(),
				};

				vk::WriteDescriptorSet write[] = {
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(array_length(uniforms))
					.setPBufferInfo(uniforms)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(2)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
			}

		}
	}

	void execute(vk::CommandBuffer cmd)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		m_gi2d_scene.m_frame = (m_gi2d_scene.m_frame + 1) % 1;
		auto reso = m_gi2d_info.m_resolution;

		uint radiance_offset = reso.x*reso.y;
		int map_offset = 0;
		uvec2 map_reso = reso.zw();
		for (int i = 0; i < m_gi2d_scene.m_hierarchy; i++)
		{
			radiance_offset >>= 2;
			map_offset += (reso.z >> i)*(reso.w >> i);
			map_reso >>= 1;
		}
		m_gi2d_scene.m_radiance_offset = radiance_offset;
		m_gi2d_scene.m_map_offset = map_offset;
		m_gi2d_scene.m_map_reso = map_reso;

		{
			vk::BufferMemoryBarrier to_write[] = {
				u_gi2d_scene.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);
		}

		cmd.updateBuffer<GI2DScene>(u_gi2d_scene.getInfo().buffer, u_gi2d_scene.getInfo().offset, m_gi2d_scene);

		{
			vk::BufferMemoryBarrier to_read[] = {
				u_gi2d_scene.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);
		}
	}

	std::shared_ptr<btr::Context> m_context;
	
	GI2DInfo m_gi2d_info;
	GI2DScene m_gi2d_scene;

	btr::BufferMemoryEx<GI2DInfo> u_gi2d_info;
	btr::BufferMemoryEx<GI2DScene> u_gi2d_scene;
	btr::BufferMemoryEx<Fragment> b_fragment;
	btr::BufferMemoryEx<uint64_t> b_fragment_map;
	btr::BufferMemoryEx<int32_t> b_grid_counter;

	std::array<vk::UniqueDescriptorSetLayout, Layout_Num> m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout(Layout i)const { return m_descriptor_set_layout[i].get(); }
};

struct GI2DSDF
{
	// https://postd.cc/voronoi-diagrams/
	enum
	{
		SDF_USE_NUM = 1,
	};
	GI2DSDF(const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_gi2d_context = gi2d_context;
		const auto& context = gi2d_context->m_context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		{
			b_jfa = context->m_storage_memory.allocateMemory<i16vec2>({ gi2d_context->FragmentBufferSize * SDF_USE_NUM,{} });
 			b_sdf = context->m_storage_memory.allocateMemory<float>({ gi2d_context->FragmentBufferSize * SDF_USE_NUM,{} });
		}

		{
			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
 					b_jfa.getInfo(),
 					b_sdf.getInfo(),
				};

				vk::WriteDescriptorSet write[] = {
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}
	}
	void execute(vk::CommandBuffer cmd)
	{
	}
	std::shared_ptr<GI2DContext> m_gi2d_context;

 	btr::BufferMemoryEx<i16vec2> b_jfa;
 	btr::BufferMemoryEx<float> b_sdf;

	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
};

struct GI2DPathContext
{
	GI2DPathContext(const std::shared_ptr<GI2DContext>& gi2d_context)
	{
		m_gi2d_context = gi2d_context;
		const auto& context = gi2d_context->m_context;
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		// descriptor
		{
			b_connect = context->m_storage_memory.allocateMemory<uint32_t>({ 1,{} });
			b_closed = context->m_storage_memory.allocateMemory<uint32_t>({ gi2d_context->FragmentBufferSize / 32,{} });
			b_neighbor = context->m_storage_memory.allocateMemory<uint8_t>({ gi2d_context->FragmentBufferSize,{} });
			b_cost = context->m_storage_memory.allocateMemory<uint32_t>(gi2d_context->FragmentBufferSize);
			b_parent = context->m_storage_memory.allocateMemory<i16vec2>(gi2d_context->FragmentBufferSize);
			b_neighbor_table = context->m_storage_memory.allocateMemory<uint8_t>(2048);
		}

		// ���O�v�Z
		{
			std::array<uint8_t, 2048> neighbor_table;
			uvec4 neighor_check_list[] =
			{
				uvec4(2,6,1,7), // diagonal_path 
				uvec4(3,5,4,4), // diagonal_wall
				uvec4(1,7,4,4), // straight_path
				uvec4(2,6,4,4), // straight_wall
			};

			for (int dir = 0; dir < 8; dir++)
			{
				for (int neighbor = 0; neighbor<256; neighbor++)
				{
					uvec4 path_check = (uvec4(dir) + neighor_check_list[(dir % 2) * 2]) % uvec4(8);
					uvec4 wall_check = (uvec4(dir) + neighor_check_list[(dir % 2) * 2 + 1]) % uvec4(8);
					uvec4 path_bit = uvec4(1) << path_check;
					uvec4 wall_bit = uvec4(1) << wall_check;

					bvec4 is_path = notEqual((uvec4(~neighbor)) & path_bit, uvec4(0));
					bvec4 is_wall = notEqual(uvec4(neighbor) & wall_bit, uvec4(0));
					uvec4 is_open = uvec4(is_path) * uvec4(is_wall.xy(), 1 - ivec2(dir % 2));

					uint is_advance = uint(((~neighbor) & (1u << dir)) != 0) * uint(any(is_path.zw()));
					uint num = is_open.x + is_open.y + is_open.z + is_open.w + is_advance;

					uint8_t neighbor_value = 0;
					for (int i = 0; i < 4; i++)
					{
						if (is_open[i] == 0) { continue; }
						neighbor_value |= path_bit[i];
					}

					if (is_advance != 0)
					{
						neighbor_value |= 1<<dir;
					}

					neighbor_table[dir*256+neighbor] = neighbor_value;
				}
			}
			cmd.updateBuffer(b_neighbor_table.getInfo().buffer, b_neighbor_table.getInfo().offset, sizeof(uint8_t)*2048, neighbor_table.data());
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { b_neighbor_table.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead) }, {});

		}
		// descriptor set
		{
			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Path),
				};
				vk::DescriptorSetAllocateInfo desc_info;
				desc_info.setDescriptorPool(context->m_descriptor_pool.get());
				desc_info.setDescriptorSetCount(array_length(layouts));
				desc_info.setPSetLayouts(layouts);
				m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

				vk::DescriptorBufferInfo storages[] = {
					b_connect.getInfo(),
					b_closed.getInfo(),
					b_neighbor.getInfo(),
					b_cost.getInfo(),
					b_parent.getInfo(),
					b_neighbor_table.getInfo(),
				};

				vk::WriteDescriptorSet write[] = {
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(array_length(storages))
					.setPBufferInfo(storages)
					.setDstBinding(0)
					.setDstSet(m_descriptor_set.get()),
				};
				context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
			}
		}
	}
	std::shared_ptr<GI2DContext> m_gi2d_context;

	btr::BufferMemoryEx<uint32_t> b_connect;
	btr::BufferMemoryEx<uint32_t> b_closed;
	btr::BufferMemoryEx<uint8_t> b_neighbor;
	btr::BufferMemoryEx<uint32_t> b_cost;
	btr::BufferMemoryEx<i16vec2> b_parent;
	btr::BufferMemoryEx<uint8_t> b_neighbor_table;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }

};
