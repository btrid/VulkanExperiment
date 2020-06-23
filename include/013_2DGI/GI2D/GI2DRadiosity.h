#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>

struct GI2DRadiosity
{
	enum {
		Frame_Num = 4,
		Dir_Num = 45,
		Bounce_Num = 2,
		Emissive_Num = 1024,
		Mesh_Num = 8,
		Mesh_Vertex_Size = 512,
	};
	enum Shader
	{
		Shader_MakeHitpoint,
		Shader_RayMarch,
		Shader_RayBounce,

		Shader_RadiosityVS,
		Shader_RadiosityGS,
		Shader_RadiosityFS,

		Shader_RenderingVS,
		Shader_RenderingFS,

		Shader_LBR_MakeVPL,
		Shader_MakeDirectLight,
		Shader_DirectLightingVS,
		Shader_DirectLightingFS,

		Shader_MakeGlobalLine,

		Shader_PixelBasedRaytracing,
		Shader_PixelBasedRaytracing2,

		Shader_Num,
	};
	enum PipelineLayout
	{
		PipelineLayout_Radiosity,
		PipelineLayout_DirectLighting,
		PipelineLayout_PixelBasedRaytracing,
		PipelineLayout_PixelBasedRaytracing2,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MakeHitpoint,
		Pipeline_RayMarch,
		Pipeline_RayBounce,
		Pipeline_Radiosity,

		Pipeline_Rendering,

		Pipeline_LBR_MakeVPL,
		Pipeline_MakeDirectLight,
		Pipeline_DirectLighting,

		Pipeline_PixelBasedRaytracing,
		Pipeline_PixelBasedRaytracing2,

		Pipeline_Num,
	};

	struct GI2DRadiosityInfo
	{
		uint ray_num_max;
		uint ray_frame_max;
		uint frame_max;
		uint frame;
	};

	struct Segment
	{
		u16vec4 pos;
	};

	struct SegmentCounter
	{
		vk::DrawIndirectCommand cmd;
		uvec4 bounce_cmd;
	};

	struct Emissive
	{
		i16vec2 pos;
		u16vec2 flag;
		f16vec4 color;
		f16vec2 angle;
	};

	GI2DRadiosity(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<GI2DContext>& gi2d_context, const std::shared_ptr<RenderTarget>& render_target)
	{
		m_context = context;
		m_gi2d_context = gi2d_context;
		m_render_target = render_target;

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

//#define RADIOSITY_TEXTURE vk::Format::eR16G16B16A16Sfloat
//#define RADIOSITY_TEXTURE vk::Format::eA2R10G10B10UnormPack32
#define RADIOSITY_TEXTURE vk::Format::eB10G11R11UfloatPack32

		m_radiosity_texture_size = gi2d_context->m_gi2d_info.m_resolution;


		{
			uint32_t size = m_gi2d_context->m_desc.Resolution.x * m_gi2d_context->m_desc.Resolution.y;
			u_radiosity_info = m_context->m_uniform_memory.allocateMemory<GI2DRadiosityInfo>({ 1,{} });
			b_segment_counter = m_context->m_storage_memory.allocateMemory<SegmentCounter>({ 1,{} });
			b_segment = m_context->m_storage_memory.allocateMemory<Segment>({ 3000000,{} });
			b_radiance = m_context->m_storage_memory.allocateMemory<uint64_t>({ size * 2,{} });
			b_edge = m_context->m_storage_memory.allocateMemory<uint64_t>({ size / 64,{} });
			b_albedo = m_context->m_storage_memory.allocateMemory<f16vec4>({ size,{} });
			b_vpl_count = m_context->m_storage_memory.allocateMemory<uint32_t>({ size,{} });
			b_vpl_index = m_context->m_storage_memory.allocateMemory<uint16_t>({ size * 64,{} });
			b_vpl_duplicate = m_context->m_storage_memory.allocateMemory<uint32_t>({ size * (Emissive_Num / 32),{} });
			b_global_line_index = m_context->m_storage_memory.allocateMemory<uvec2>((m_gi2d_context->m_desc.Resolution.x * 2 + m_gi2d_context->m_desc.Resolution.y * 2)* Dir_Num);
			b_global_line_data = m_context->m_storage_memory.allocateMemory<u16vec2>(size * Dir_Num);
			v_emissive = m_context->m_vertex_memory.allocateMemory<Emissive>(Emissive_Num);
			u_circle_mesh_count = m_context->m_uniform_memory.allocateMemory<uint32_t>(Mesh_Num);
			u_circle_mesh_vertex = m_context->m_uniform_memory.allocateMemory<i16vec2>(Mesh_Num*Mesh_Vertex_Size);
			v_emissive_draw_command = m_context->m_vertex_memory.allocateMemory<vk::DrawIndirectCommand>(Emissive_Num);
			v_emissive_draw_count = m_context->m_vertex_memory.allocateMemory<uint32_t>(1);

			m_info.ray_num_max = 0;
			m_info.ray_frame_max = 0;
			m_info.frame_max = Frame_Num;
			m_info.frame = 3;
			cmd.updateBuffer<GI2DRadiosityInfo>(u_radiosity_info.getInfo().buffer, u_radiosity_info.getInfo().offset, m_info);
			vk::BufferMemoryBarrier to_read[] = {
				u_radiosity_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTopOfPipe, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);


			vk::ImageCreateInfo image_info;
			image_info.setExtent(vk::Extent3D(m_radiosity_texture_size.x, m_radiosity_texture_size.y, 1));
			image_info.setArrayLayers(Frame_Num);
			image_info.setFormat(RADIOSITY_TEXTURE);
			image_info.setImageType(vk::ImageType::e2D);
			image_info.setInitialLayout(vk::ImageLayout::eUndefined);
			image_info.setMipLevels(1);
			image_info.setSamples(vk::SampleCountFlagBits::e1);
			image_info.setSharingMode(vk::SharingMode::eExclusive);
			image_info.setTiling(vk::ImageTiling::eOptimal);
			image_info.setUsage(vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);

			m_image = m_context->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = context->m_device.getImageMemoryRequirements(m_image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(context->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			m_image_memory = context->m_device.allocateMemoryUnique(memory_alloc_info);
			context->m_device.bindImageMemory(m_image.get(), m_image_memory.get(), 0);

			vk::ImageViewCreateInfo view_info;
			view_info.setFormat(image_info.format);
			view_info.setImage(m_image.get());
			view_info.subresourceRange.setLayerCount(1);
			view_info.subresourceRange.setBaseMipLevel(0);
			view_info.subresourceRange.setLevelCount(1);
			view_info.subresourceRange.setAspectMask(vk::ImageAspectFlagBits::eColor);
			view_info.setViewType(vk::ImageViewType::e2D);
			view_info.components.setR(vk::ComponentSwizzle::eR).setG(vk::ComponentSwizzle::eG).setB(vk::ComponentSwizzle::eB);

			for (int i = 0; i < Frame_Num; i++)
			{
				view_info.subresourceRange.setBaseArrayLayer(i);
				m_image_view[i] = m_context->m_device.createImageViewUnique(view_info);
			}

			view_info.subresourceRange.setBaseArrayLayer(0);
			view_info.subresourceRange.setLayerCount(Frame_Num);
			view_info.setViewType(vk::ImageViewType::e2D);
			m_image_rtv = m_context->m_device.createImageViewUnique(view_info);

			vk::SamplerCreateInfo sampler_info;
			sampler_info.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
			sampler_info.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
			sampler_info.setAddressModeW(vk::SamplerAddressMode::eClampToEdge);
			sampler_info.setAnisotropyEnable(false);
			sampler_info.setMagFilter(vk::Filter::eLinear);
			sampler_info.setMinFilter(vk::Filter::eLinear);
			sampler_info.setMinLod(0.f);
			sampler_info.setMaxLod(0.f);
			sampler_info.setMipLodBias(0.f);
			sampler_info.setMipmapMode(vk::SamplerMipmapMode::eNearest);
			sampler_info.setUnnormalizedCoordinates(false);
			m_image_sampler = m_context->m_device.createSamplerUnique(sampler_info);

			vk::ImageMemoryBarrier to_copy_barrier;
			to_copy_barrier.image = m_image.get();
			to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
			to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
			to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_copy_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, Frame_Num };

			vk::ImageMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.image = m_image.get();
			to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			to_shader_read_barrier.subresourceRange = vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, Frame_Num };

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
			cmd.clearColorImage(m_image.get(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(std::array<uint32_t, 4>{0}), vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, Frame_Num });
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });

#if USE_DEBUG_REPORT
			context->m_device.setDebugUtilsObjectNameEXT({ vk::ObjectType::eImage, reinterpret_cast<uint64_t &>(m_image.get()), "GI2DRadiosity::m_image" }, context->m_dispach);
			context->m_device.setDebugUtilsObjectNameEXT({ vk::ObjectType::eImageView, reinterpret_cast<uint64_t &>(m_image_rtv.get()), "GI2DRadiosity::m_imag_rtv" }, context->m_dispach);
			context->m_device.setDebugUtilsObjectNameEXT({ vk::ObjectType::eImageView, reinterpret_cast<uint64_t &>(m_image_view[0].get()), "GI2DRadiosity::m_image_view[0]" }, context->m_dispach);
			context->m_device.setDebugUtilsObjectNameEXT({ vk::ObjectType::eImageView, reinterpret_cast<uint64_t &>(m_image_view[1].get()), "GI2DRadiosity::m_image_view[1]" }, context->m_dispach);
			context->m_device.setDebugUtilsObjectNameEXT({ vk::ObjectType::eImageView, reinterpret_cast<uint64_t &>(m_image_view[2].get()), "GI2DRadiosity::m_image_view[2]" }, context->m_dispach);
			context->m_device.setDebugUtilsObjectNameEXT({ vk::ObjectType::eImageView, reinterpret_cast<uint64_t &>(m_image_view[3].get()), "GI2DRadiosity::m_image_view[3]" }, context->m_dispach);
			context->m_device.setDebugUtilsObjectNameEXT({ vk::ObjectType::eSampler, reinterpret_cast<uint64_t &>(m_image_sampler.get()), "GI2DRadiosity::m_image_sampler" }, context->m_dispach);
#endif

			{
				std::vector<uint32_t> count(Mesh_Num);
				std::vector<i16vec2> data(Mesh_Vertex_Size);
				for (int i = 0; i < Mesh_Num-1; i++)
				{
					int R = glm::sqrt((i+1)*(i+1))*100.f + 1;
					int x = R;
					int y = 0;
					int err = 0;
					int dedx = (R << 1) - 1;	// 2x-1 = 2R-1
					int dedy = 1;	// 2y-1

					data[y] = i16vec2(R, 0);

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
							if (x<y)
							{
								x=y;
							}
						}
						data[y/2] = i16vec2(x, y);
					}
					cmd.updateBuffer<i16vec2>(u_circle_mesh_vertex.getInfo().buffer, u_circle_mesh_vertex.getInfo().offset + sizeof(i16vec2) * Mesh_Vertex_Size * i, data);
					count[i] = y/2;
				}

// 				ivec2 target = ivec2(0);
// 				for(int i = 0; i < (count[0])*8+2; i++)
// 				{
// 					if (i == 0)continue;
// 					uint vertex_num = count[0];
// 					uint target_ID = (i - 1) % vertex_num;
// 					uint target_type = (i - 1) / vertex_num;
// 					uint vertex_index = ((target_type % 2) == 0) ? target_ID : (vertex_num - target_ID);
// 					target = ivec2(data[vertex_index]);
// 					switch (target_type % 8)
// 					{
// 						case 0: target = ivec2(target.x, target.y); break;
// 						case 1: target = ivec2(target.y, target.x); break;
// 						case 2: target = ivec2(-target.y, target.x); break;
// 						case 3: target = ivec2(-target.x, target.y); break;
// 						case 4: target = ivec2(-target.x, -target.y); break;
// 						case 5: target = ivec2(-target.y, -target.x); break;
// 						case 6: target = ivec2(target.y, -target.x); break;
// 						case 7: target = ivec2(target.x, -target.y); break;
// 					}
// 					printf("vi=%3d, id=%3d, type=%3d, pos=[%3d,%3d]\n", vertex_index, target_ID, target_type, target.x, target.y);
// 				}
				count[Mesh_Num - 1] = gi2d_context->m_desc.Resolution.x*gi2d_context->m_desc.Resolution.y / 8;
				cmd.updateBuffer<uint32_t>(u_circle_mesh_count.getInfo().buffer, u_circle_mesh_count.getInfo().offset, count);
			}
		}

		// descriptor layout
		{
			auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
			vk::DescriptorSetLayoutBinding binding[] = 
			{
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(13, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(14, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(15, vk::DescriptorType::eStorageBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(16, vk::DescriptorType::eCombinedImageSampler, Frame_Num, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_descriptor_set_layout = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}
		// descriptor set
		{
			vk::DescriptorSetLayout layouts[] = {
				m_descriptor_set_layout.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(context->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_descriptor_set = std::move(context->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] = {
				u_radiosity_info.getInfo(),
				u_circle_mesh_count.getInfo(),
				u_circle_mesh_vertex.getInfo(),
			};
			vk::DescriptorBufferInfo storages[] = {
				b_segment_counter.getInfo(),
				b_segment.getInfo(),
				b_radiance.getInfo(),
				b_edge.getInfo(),
				b_albedo.getInfo(),
				b_vpl_count.getInfo(),
				b_vpl_index.getInfo(),
				b_vpl_duplicate.getInfo(),
				v_emissive.getInfo(),
				v_emissive_draw_command.getInfo(),
				v_emissive_draw_count.getInfo(),
				b_global_line_index.getInfo(),
				b_global_line_data.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
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
				.setDstBinding(3)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

			std::array<vk::WriteDescriptorSet, Frame_Num> write_desc;
			std::array<vk::DescriptorImageInfo, Frame_Num> image_info;
			for (int i = 0; i < Frame_Num; i++)
			{
				image_info[i] = vk::DescriptorImageInfo(m_image_sampler.get(), m_image_view[i].get(), vk::ImageLayout::eShaderReadOnlyOptimal);

				write_desc[i] =
					vk::WriteDescriptorSet()
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setDescriptorCount(1)
					.setPImageInfo(&image_info[i])
					.setDstBinding(16)
					.setDstArrayElement(i)
					.setDstSet(m_descriptor_set.get());
			}
			context->m_device.updateDescriptorSets(array_length(write_desc), write_desc.data(), 0, nullptr);
		}

		// shader
		{
			const char* name[] =
			{
				"Radiosity_MakeVertex.comp.spv",
				"Radiosity_RayMarch.comp.spv",
				"Radiosity_RayBounce.comp.spv",

				"Radiosity_Radiosity.vert.spv",
				"Radiosity_Radiosity.geom.spv",
				"Radiosity_Radiosity.frag.spv",

				"Radiosity_Rendering.vert.spv",
				"Radiosity_Rendering.frag.spv",

				"LBR_MakeVPL.vert.spv",
				"Radiosity_MakeDirectLight.comp.spv",
				"Radiosity_DirectLighting.vert.spv",
				"Radiosity_DirectLighting.frag.spv",

				"LBR_MakeGlobalLine.comp.spv",

				"Radiosity_PixelBasedRaytracing.comp.spv",
				"Radiosity_PixelBasedRaytracing2.comp.spv",

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
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					m_descriptor_set_layout.get(),
				};
				vk::PushConstantRange ranges[] = {
					vk::PushConstantRange().setSize(4).setStageFlags(vk::ShaderStageFlagBits::eCompute),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				pipeline_layout_info.setPushConstantRangeCount(array_length(ranges));
				pipeline_layout_info.setPPushConstantRanges(ranges);
				m_pipeline_layout[PipelineLayout_Radiosity] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}
			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					m_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_DirectLighting] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);

			}

			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					m_descriptor_set_layout.get(),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_PixelBasedRaytracing] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

			{
				vk::DescriptorSetLayout layouts[] = {
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_Data),
					gi2d_context->getDescriptorSetLayout(GI2DContext::Layout_SDF),
					m_descriptor_set_layout.get(),
					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_pipeline_layout[PipelineLayout_PixelBasedRaytracing2] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
			std::array<vk::PipelineShaderStageCreateInfo, 6> shader_info;
			shader_info[0].setModule(m_shader[Shader_MakeHitpoint].get());
			shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[0].setPName("main");
			shader_info[1].setModule(m_shader[Shader_RayMarch].get());
			shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[1].setPName("main");
			shader_info[2].setModule(m_shader[Shader_RayBounce].get());
			shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[2].setPName("main");
			shader_info[3].setModule(m_shader[Shader_MakeDirectLight].get());
			shader_info[3].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[3].setPName("main");
			shader_info[4].setModule(m_shader[Shader_PixelBasedRaytracing].get());
			shader_info[4].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[4].setPName("main");
			shader_info[5].setModule(m_shader[Shader_PixelBasedRaytracing2].get());
			shader_info[5].setStage(vk::ShaderStageFlagBits::eCompute);
			shader_info[5].setPName("main");
			std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
			{
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[0])
				.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[1])
				.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[2])
				.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[3])
				.setLayout(m_pipeline_layout[PipelineLayout_DirectLighting].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[4])
				.setLayout(m_pipeline_layout[PipelineLayout_PixelBasedRaytracing].get()),
				vk::ComputePipelineCreateInfo()
				.setStage(shader_info[5])
				.setLayout(m_pipeline_layout[PipelineLayout_PixelBasedRaytracing2].get()),
			};
			auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
			m_pipeline[Pipeline_MakeHitpoint] = std::move(compute_pipeline[0]);
			m_pipeline[Pipeline_RayMarch] = std::move(compute_pipeline[1]);
			m_pipeline[Pipeline_RayBounce] = std::move(compute_pipeline[2]);
			m_pipeline[Pipeline_MakeDirectLight] = std::move(compute_pipeline[3]);
			m_pipeline[Pipeline_PixelBasedRaytracing] = std::move(compute_pipeline[4]);
			m_pipeline[Pipeline_PixelBasedRaytracing2] = std::move(compute_pipeline[5]);
		}

		// レンダーパス
		{
			vk::AttachmentReference color_ref[] = {
				vk::AttachmentReference().setLayout(vk::ImageLayout::eColorAttachmentOptimal).setAttachment(0),
			};

			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			vk::AttachmentDescription attach_desc[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(RADIOSITY_TEXTURE)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_desc));
			renderpass_info.setPAttachments(attach_desc);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_radiosity_pass = context->m_device.createRenderPassUnique(renderpass_info);

			{
				vk::ImageView view[] = {
					m_image_rtv.get(),
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_radiosity_pass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(m_radiosity_texture_size.x);
				framebuffer_info.setHeight(m_radiosity_texture_size.y);
				framebuffer_info.setLayers(Frame_Num);

				m_radiosity_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}
		}
		{
			vk::AttachmentReference color_ref[] = {
				vk::AttachmentReference().setLayout(vk::ImageLayout::eColorAttachmentOptimal).setAttachment(0),
			};

			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			vk::AttachmentDescription attach_desc[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(render_target->m_info.format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_desc));
			renderpass_info.setPAttachments(attach_desc);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_rendering_pass = context->m_device.createRenderPassUnique(renderpass_info);
			{
				vk::ImageView view[] = {
					render_target->m_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_rendering_pass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(render_target->m_info.extent.width);
				framebuffer_info.setHeight(render_target->m_info.extent.height);
				framebuffer_info.setLayers(1);

				m_rendering_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}
		}

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

			m_make_vpl_renderpass = context->m_device.createRenderPassUnique(renderpass_info);
			{
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_make_vpl_renderpass.get());
				framebuffer_info.setAttachmentCount(0);
				framebuffer_info.setPAttachments(nullptr);
				framebuffer_info.setWidth(render_target->m_info.extent.width);
				framebuffer_info.setHeight(render_target->m_info.extent.height);
				framebuffer_info.setLayers(1);

				m_make_vpl_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}
		}

		// graphics pipeline
		{
			vk::PipelineShaderStageCreateInfo shader_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_RadiosityVS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_RadiosityGS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eGeometry),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_RadiosityFS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			vk::PipelineShaderStageCreateInfo shader_rendering_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_RenderingVS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_RenderingFS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};	
			
			vk::PipelineShaderStageCreateInfo shader_direct_light_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_DirectLightingVS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_DirectLightingFS].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eFragment),
			};

			vk::PipelineShaderStageCreateInfo shader_make_vpl_info[] =
			{
				vk::PipelineShaderStageCreateInfo()
				.setModule(m_shader[Shader_LBR_MakeVPL].get())
				.setPName("main")
				.setStage(vk::ShaderStageFlagBits::eVertex),
			};

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

			vk::PipelineInputAssemblyStateCreateInfo assembly_info2;
			assembly_info2.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info2.setTopology(vk::PrimitiveTopology::eTriangleList);

			vk::PipelineInputAssemblyStateCreateInfo assembly_info3;
			assembly_info3.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info3.setTopology(vk::PrimitiveTopology::eTriangleFan);

			// viewport
			vk::Viewport viewport = vk::Viewport(0.f, 0.f, (float)m_radiosity_texture_size.x, (float)m_radiosity_texture_size.y, 0.f, 1.f);
			vk::Rect2D scissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_radiosity_texture_size.x, m_radiosity_texture_size.y));
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(1);
			viewportInfo.setPViewports(&viewport);
			viewportInfo.setScissorCount(1);
			viewportInfo.setPScissors(&scissor);

			vk::Viewport viewport2 = vk::Viewport(0.f, 0.f, (float)render_target->m_resolution.width, (float)render_target->m_resolution.height, 0.f, 1.f);
			vk::Rect2D scissor2 = vk::Rect2D(vk::Offset2D(0, 0), render_target->m_resolution);
			vk::PipelineViewportStateCreateInfo viewportInfo2;
			viewportInfo2.setViewportCount(1);
			viewportInfo2.setPViewports(&viewport2);
			viewportInfo2.setScissorCount(1);
			viewportInfo2.setPScissors(&scissor2);

			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);

			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);


			vk::PipelineColorBlendAttachmentState blend_state;
			blend_state.setBlendEnable(VK_TRUE);
			blend_state.setColorBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcColorBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstColorBlendFactor(vk::BlendFactor::eOne);
			blend_state.setAlphaBlendOp(vk::BlendOp::eAdd);
			blend_state.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
			blend_state.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
			blend_state.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
			/*| vk::ColorComponentFlagBits::eA*/);

			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(1);
			blend_info.setPAttachments(&blend_state);

			vk::PipelineColorBlendAttachmentState blend_state2;
			blend_state2.setBlendEnable(VK_TRUE);
			blend_state2.setColorBlendOp(vk::BlendOp::eAdd);
			blend_state2.setSrcColorBlendFactor(vk::BlendFactor::eOne);
			blend_state2.setDstColorBlendFactor(vk::BlendFactor::eOne);
			blend_state2.setAlphaBlendOp(vk::BlendOp::eAdd);
			blend_state2.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
			blend_state2.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
			blend_state2.setColorWriteMask(vk::ColorComponentFlagBits::eR
				| vk::ColorComponentFlagBits::eG
				| vk::ColorComponentFlagBits::eB
			/*| vk::ColorComponentFlagBits::eA*/);
			vk::PipelineColorBlendStateCreateInfo blend_info2;
			blend_info2.setAttachmentCount(1);
			blend_info2.setPAttachments(&blend_state2);

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::PipelineVertexInputStateCreateInfo direct_light_vertex_input_info;
			vk::VertexInputBindingDescription directlight_vinput_binding_desc[] = {
				vk::VertexInputBindingDescription(0, sizeof(Emissive), vk::VertexInputRate::eInstance),
			};
			vk::VertexInputAttributeDescription directlight_vinput_attr_desc[] = {
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR16G16Sint, offsetof(Emissive, pos)),
				vk::VertexInputAttributeDescription(1, 0, vk::Format::eR16G16Uint, offsetof(Emissive, flag)),
				vk::VertexInputAttributeDescription(2, 0, vk::Format::eR16G16B16A16Sfloat, offsetof(Emissive, color)),
				vk::VertexInputAttributeDescription(3, 0, vk::Format::eR16G16Sfloat, offsetof(Emissive, angle)),
			};
			direct_light_vertex_input_info.setVertexBindingDescriptionCount(array_length(directlight_vinput_binding_desc));
			direct_light_vertex_input_info.setPVertexBindingDescriptions(directlight_vinput_binding_desc);
			direct_light_vertex_input_info.setVertexAttributeDescriptionCount(array_length(directlight_vinput_attr_desc));
			direct_light_vertex_input_info.setPVertexAttributeDescriptions(directlight_vinput_attr_desc);

			std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
			{
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_info))
				.setPStages(shader_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get())
				.setRenderPass(m_radiosity_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_rendering_info))
				.setPStages(shader_rendering_info)
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info2)
				.setPViewportState(&viewportInfo2)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayout_Radiosity].get())
				.setRenderPass(m_rendering_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info2),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_direct_light_info))
				.setPStages(shader_direct_light_info)
				.setPVertexInputState(&direct_light_vertex_input_info)
				.setPInputAssemblyState(&assembly_info3)
				.setPViewportState(&viewportInfo2)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayout_DirectLighting].get())
				.setRenderPass(m_rendering_pass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info2),
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(array_length(shader_make_vpl_info))
				.setPStages(shader_make_vpl_info)
				.setPVertexInputState(&direct_light_vertex_input_info)
				.setPInputAssemblyState(&assembly_info3)
				.setPViewportState(&viewportInfo2)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_pipeline_layout[PipelineLayout_DirectLighting].get())
				.setRenderPass(m_make_vpl_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info2),
			};
			auto graphics_pipeline = context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info);
			m_pipeline[Pipeline_Radiosity] = std::move(graphics_pipeline[0]);
			m_pipeline[Pipeline_Rendering] = std::move(graphics_pipeline[1]);
			m_pipeline[Pipeline_DirectLighting] = std::move(graphics_pipeline[2]);
			m_pipeline[Pipeline_LBR_MakeVPL] = std::move(graphics_pipeline[3]);
		}

	}

	void executeGlobalLineRadiosity(const vk::CommandBuffer& cmd)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		vk::DescriptorSet desc[] = {
			m_gi2d_context->getDescriptorSet(),
			m_descriptor_set.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, array_length(desc), desc, 0, nullptr);

		// データクリア
		{
			vk::BufferMemoryBarrier to_write[] = {
				b_edge.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				b_segment_counter.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead, vk::AccessFlagBits::eTransferWrite),
				u_radiosity_info.makeMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferWrite),
			};

			vk::ImageMemoryBarrier image_write;
			image_write.setImage(m_image.get());
			image_write.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, Frame_Num });
			image_write.setOldLayout(vk::ImageLayout::eUndefined);
			//		image_barrier.setSrcAccessMask(vk::AccessFlagBits::eMemoryRead);
			image_write.setNewLayout(vk::ImageLayout::eTransferDstOptimal);
			image_write.setDstAccessMask(vk::AccessFlagBits::eTransferWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_write), to_write, 1, &image_write);

			cmd.updateBuffer<SegmentCounter>(b_segment_counter.getInfo().buffer, b_segment_counter.getInfo().offset, SegmentCounter{ { 1, 0, 0, 0 },{ 0, 1, 1, 0 } });
			cmd.fillBuffer(b_edge.getInfo().buffer, b_edge.getInfo().offset, b_edge.getInfo().range, 0);

			m_info.frame = (m_info.frame + 1) % m_info.frame_max;
			cmd.updateBuffer<GI2DRadiosityInfo>(u_radiosity_info.getInfo().buffer, u_radiosity_info.getInfo().offset, m_info);


			vk::ImageSubresourceRange subresource;
			subresource.setBaseArrayLayer(m_info.frame);
			subresource.setLayerCount(1);
			subresource.setBaseMipLevel(0);
			subresource.setLevelCount(1);
			subresource.setAspectMask(vk::ImageAspectFlagBits::eColor);
			cmd.clearColorImage(m_image.get(), vk::ImageLayout::eTransferDstOptimal, vk::ClearColorValue(std::array<uint32_t, 4>{0}), subresource);
		}

		// レイのヒットポイント生成
		_label.insert("GI2DRadiosity::executeMakeVertex");
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_segment_counter.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				b_edge.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				u_radiosity_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),

			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eGeometryShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeHitpoint].get());
			auto num = app::calcDipatchGroups(uvec3(m_gi2d_context->m_desc.Resolution.x, m_gi2d_context->m_desc.Resolution.y, 1), uvec3(32, 32, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// レイの範囲の生成
		_label.insert("GI2DRadiosity::executeRayMarch");
		{
			vk::BufferMemoryBarrier to_read[] = {
				m_gi2d_context->b_fragment_map.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
				b_edge.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayMarch].get());

			cmd.pushConstants<float>(m_pipeline_layout[PipelineLayout_Radiosity].get(), vk::ShaderStageFlagBits::eCompute, 0, 0.25f);
			
			uint direction_ray_num = glm::max(m_radiosity_texture_size.x, m_radiosity_texture_size.y);
			auto num = app::calcDipatchGroups(uvec3(direction_ray_num * 2, Dir_Num, 1), uvec3(128, 1, 1));
			cmd.dispatch(num.x, num.y, num.z);
		}

		// bounce
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_segment_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
				b_albedo.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eDrawIndirect| vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);

			for (int i = 0; i < Bounce_Num; i++)
			{
				_label.insert("GI2DRadiosity::executeBounce");
				{
					vk::BufferMemoryBarrier to_read[] =
					{
						b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
					};
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_read), to_read, 0, nullptr);
					cmd.pushConstants<int>(m_pipeline_layout[PipelineLayout_Radiosity].get(), vk::ShaderStageFlagBits::eCompute, 0, i%2);

					cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_RayBounce].get());
					cmd.dispatchIndirect(b_segment_counter.getInfo().buffer, b_segment_counter.getInfo().offset + offsetof(SegmentCounter, bounce_cmd));
				}

			}

		}

		// render_targetに書く
		_label.insert("GI2DRadiosity::executeRendering");
		{
			vk::BufferMemoryBarrier to_read[] = {
				b_radiance.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead),
			};

			vk::ImageMemoryBarrier image_barrier;
			image_barrier.setImage(m_image.get());
			image_barrier.setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, Frame_Num });
			image_barrier.setOldLayout(vk::ImageLayout::eTransferDstOptimal);
			image_barrier.setSrcAccessMask(vk::AccessFlagBits::eTransferWrite);
			image_barrier.setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eGeometryShader | vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { array_size(to_read), to_read }, { image_barrier });
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_radiosity_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_radiosity_texture_size.x, m_radiosity_texture_size.y)));
		begin_render_Info.setFramebuffer(m_radiosity_framebuffer.get());
		begin_render_Info.setClearValueCount(1);
		auto color = vk::ClearValue(vk::ClearColorValue(std::array<uint32_t, 4>{}));
		begin_render_Info.setPClearValues(&color);
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, m_gi2d_context->getDescriptorSet(), {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Radiosity].get(), 1, m_descriptor_set.get(), {});

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Radiosity].get());
		cmd.drawIndirect(b_segment_counter.getInfo().buffer, b_segment_counter.getInfo().offset, 1, sizeof(SegmentCounter));

		cmd.endRenderPass();


	}

	void executeGlobalLineRadiosityRendering(const vk::CommandBuffer& cmd)
	{

		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		// render_targetに書く
		{

			std::array<vk::ImageMemoryBarrier, 2> image_barrier;
			image_barrier[0].setImage(m_render_target->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[1].setImage(m_image.get());
			image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, Frame_Num });
			image_barrier[1].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
			image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_rendering_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
		begin_render_Info.setFramebuffer(m_rendering_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Rendering].get());
		vk::DescriptorSet descs[] = {
			m_gi2d_context->getDescriptorSet(),
			m_descriptor_set.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_Radiosity].get(), 0, array_length(descs), descs, 0, nullptr);
		cmd.draw(3, 1, 0, 0);

		cmd.endRenderPass();

	}

	void executeLightBasedRaytracing(const vk::CommandBuffer& cmd)
	{

		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		// ライトデータ作成
		static std::array<Emissive, Emissive_Num> s_data;
		{
			static std::once_flag s_is_init_light;
			std::call_once(s_is_init_light, []()
			{
				std::vector<vec4> colors = { vec4(1.f, 0.f, 0.f, 0.f), vec4(0.f, 1.f, 0.f, 0.f) , vec4(0.f, 0.f, 1.f, 0.f), vec4(0.f) };
				for (int i = 0; i < s_data.size(); i++)
				{
					auto color_index = std::rand() % 3;
//					color_index = 3;
					s_data[i] = Emissive{ i16vec2(std::rand() % 950 + 40, std::rand() % 950 + 40), u8vec4(), glm::packHalf4x16(colors[color_index]*10.f), glm::packHalf2x16(vec2(0.f, 1.f)) };
				}
			});

			static vec2 light_pos = vec2(646.5f, 601.6f);
			static int light_power = 10;
			static float light_dir = 0.f;
			static float light_angle = 1.f;
			float move = 3.f;
			if (m_context->m_window->getInput().m_keyboard.isHold('A')) { move = 0.03f; }
			light_pos.x += m_context->m_window->getInput().m_keyboard.isHold(VK_RIGHT) * move;
			light_pos.x -= m_context->m_window->getInput().m_keyboard.isHold(VK_LEFT) * move;
			light_pos.y -= m_context->m_window->getInput().m_keyboard.isHold(VK_UP) * move;
			light_pos.y += m_context->m_window->getInput().m_keyboard.isHold(VK_DOWN) * move;
			if (m_context->m_window->getInput().m_keyboard.isOn(' ')) { light_power = (light_power + 5) % 60; }
//			if (m_context->m_window->getInput().m_keyboard.isOn('D')) { light_dir = glm::mod(light_dir + 0.05f, 1.f); }
//			if (m_context->m_window->getInput().m_keyboard.isOn('F')) { light_angle = glm::mod(light_angle + 0.05f, 1.f); }

//			s_data[0].pos = i16vec2(light_pos);
	//		s_data[0].color = glm::packHalf4x16(vec4(light_power+0.3f));
	//		s_data[0].angle = glm::packHalf2x16(vec2(light_dir, light_angle));
		}
		// emissiveデータ作成
		_label.insert("GI2DRadiosity::executeMakeDirectLight");
		{
			vk::BufferMemoryBarrier to_write[] =
			{
				v_emissive.makeMemoryBarrier(vk::AccessFlagBits::eVertexAttributeRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect |vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.updateBuffer<Emissive>(v_emissive.getInfo().buffer, v_emissive.getInfo().offset, s_data);
		}

		// draw command 作成
		{
			{
				vk::BufferMemoryBarrier to_init[] = {
					v_emissive_draw_count.makeMemoryBarrier(vk::AccessFlagBits::eIndirectCommandRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eDrawIndirect, vk::PipelineStageFlagBits::eTransfer, {}, 0, nullptr, array_length(to_init), to_init, 0, nullptr);
				cmd.fillBuffer(v_emissive_draw_count.getInfo().buffer, v_emissive_draw_count.getInfo().offset, 4, 0);

			}
			{
				vk::BufferMemoryBarrier to_write[] = {
					v_emissive_draw_count.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
					v_emissive.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, 0, nullptr, array_length(to_write), to_write, 0, nullptr);

				vk::DescriptorSet descs[] = {
					m_gi2d_context->getDescriptorSet(),
					m_descriptor_set.get(),
				};
				cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_DirectLighting].get(), 0, array_length(descs), descs, 0, nullptr);

				cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeDirectLight].get());
				cmd.dispatch(1, 1, 1);
			}

		}
		
		// virtualpointlight生成
		{
			// clear vpl
			{
				vk::BufferMemoryBarrier to_clear[] =
				{
					b_vpl_count.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
					b_vpl_count.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {}, {}, { array_length(to_clear), to_clear }, {});

				cmd.fillBuffer(b_vpl_count.getInfo().buffer, b_vpl_count.getInfo().offset, b_vpl_count.getInfo().range, 0);
				cmd.fillBuffer(b_vpl_duplicate.getInfo().buffer, b_vpl_duplicate.getInfo().offset, b_vpl_duplicate.getInfo().range, 0);

				vk::BufferMemoryBarrier to_write[] =
				{
					b_vpl_count.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
					b_vpl_count.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite),
				};
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, { array_length(to_write), to_write }, {});
			}
			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_make_vpl_renderpass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
			begin_render_Info.setFramebuffer(m_make_vpl_framebuffer.get());
			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_LBR_MakeVPL].get());
			vk::DescriptorSet descs[] = {
				m_gi2d_context->getDescriptorSet(),
				m_descriptor_set.get(),
			};
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_DirectLighting].get(), 0, array_length(descs), descs, 0, nullptr);
			vk::Buffer vertex_buffers[] =
			{
				v_emissive.getInfo().buffer,
			};
			vk::DeviceSize offsets[] = { v_emissive.getInfo().offset };

			cmd.bindVertexBuffers(0, array_length(vertex_buffers), vertex_buffers, offsets);

			cmd.drawIndirect(v_emissive_draw_command.getInfo().buffer, v_emissive_draw_command.getInfo().offset, Emissive_Num, sizeof(vk::DrawIndirectCommand));
			//		cmd.drawIndirectCount(v_emissive_draw_command.getInfo().buffer, v_emissive_draw_command.getInfo().offset, v_emissive_draw_count.getInfo().buffer, v_emissive_draw_count.getInfo().offset, Emissive_Num, sizeof(vk::DrawIndirectCommand));
			cmd.endRenderPass();
		}
		// render_targetに書く
		{
			vk::BufferMemoryBarrier to_read[] =
			{
				v_emissive.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eVertexAttributeRead),
				v_emissive_draw_command.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
				v_emissive_draw_count.makeMemoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eIndirectCommandRead),
			};

			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
			image_barrier[0].setImage(m_render_target->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
// 			image_barrier[1].setImage(m_image.get());
// 			image_barrier[1].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, Frame_Num });
// 			image_barrier[1].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
// 			image_barrier[1].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
// 			image_barrier[1].setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
// 			image_barrier[1].setDstAccessMask(vk::AccessFlagBits::eShaderRead);

			auto src_stage_mask = vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eComputeShader;
			auto dst_stage_mask = vk::PipelineStageFlagBits::eDrawIndirect | vk::PipelineStageFlagBits::eVertexInput | vk::PipelineStageFlagBits::eColorAttachmentOutput;
			cmd.pipelineBarrier(src_stage_mask, dst_stage_mask, {}, {}, { array_length(to_read), to_read }, { array_length(image_barrier), image_barrier.data() });
		}

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_rendering_pass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), m_render_target->m_resolution));
		begin_render_Info.setFramebuffer(m_rendering_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_DirectLighting].get());
		vk::DescriptorSet descs[] = {
			m_gi2d_context->getDescriptorSet(),
			m_descriptor_set.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout[PipelineLayout_DirectLighting].get(), 0, array_length(descs), descs, 0, nullptr);
		vk::Buffer vertex_buffers[] =
		{
			v_emissive.getInfo().buffer,
		};
		vk::DeviceSize offsets[] = { v_emissive.getInfo().offset };

		cmd.bindVertexBuffers(0, array_length(vertex_buffers), vertex_buffers, offsets);

		cmd.drawIndirect(v_emissive_draw_command.getInfo().buffer, v_emissive_draw_command.getInfo().offset, Emissive_Num, sizeof(vk::DrawIndirectCommand));
//		cmd.drawIndirectCount(v_emissive_draw_command.getInfo().buffer, v_emissive_draw_command.getInfo().offset, v_emissive_draw_count.getInfo().buffer, v_emissive_draw_count.getInfo().offset, Emissive_Num, sizeof(vk::DrawIndirectCommand));
		cmd.endRenderPass();

	}

	void executePixelBasedRaytracing(const vk::CommandBuffer& cmd)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		// render_targetに書く
		{

			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
			image_barrier[0].setImage(m_render_target->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_PixelBasedRaytracing].get());
		vk::DescriptorSet descs[] = 
		{
			m_gi2d_context->getDescriptorSet(),
			m_descriptor_set.get(),
			m_render_target->m_descriptor.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_PixelBasedRaytracing].get(), 0, array_length(descs), descs, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_PixelBasedRaytracing].get());
		cmd.dispatch(m_gi2d_context->m_gi2d_info.m_resolution.x, m_gi2d_context->m_gi2d_info.m_resolution.y, 1);

	}

	void executePixelBasedRaytracing2(const vk::CommandBuffer& cmd, const std::shared_ptr<GI2DSDF>& gi2d_sdf)
	{
		DebugLabel _label(cmd, m_context->m_dispach, __FUNCTION__);

		// render_targetに書く
		{

			std::array<vk::ImageMemoryBarrier, 1> image_barrier;
			image_barrier[0].setImage(m_render_target->m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setNewLayout(vk::ImageLayout::eGeneral);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eShaderWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {}, { array_length(image_barrier), image_barrier.data() });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_PixelBasedRaytracing2].get());
		vk::DescriptorSet descs[] =
		{
			m_gi2d_context->getDescriptorSet(),
			gi2d_sdf->m_descriptor_set.get(),
			m_descriptor_set.get(),
			m_render_target->m_descriptor.get(),
		};
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_PixelBasedRaytracing2].get(), 0, array_length(descs), descs, 0, nullptr);

		cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_PixelBasedRaytracing2].get());
//		cmd.dispatch(m_gi2d_context->m_gi2d_info.m_resolution.x, m_gi2d_context->m_gi2d_info.m_resolution.y, 1);
		cmd.dispatch(m_gi2d_context->m_gi2d_info.m_resolution.x / 8, m_gi2d_context->m_gi2d_info.m_resolution.y/8, 1);

	}
	std::shared_ptr<btr::Context> m_context;
	std::shared_ptr<GI2DContext> m_gi2d_context;
	std::shared_ptr<RenderTarget> m_render_target;

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	btr::BufferMemoryEx<GI2DRadiosityInfo> u_radiosity_info;
	btr::BufferMemoryEx<SegmentCounter> b_segment_counter;
	btr::BufferMemoryEx<Segment> b_segment;
	btr::BufferMemoryEx<uint64_t> b_radiance;
	btr::BufferMemoryEx<uint64_t> b_edge;
	btr::BufferMemoryEx<f16vec4> b_albedo;
	btr::BufferMemoryEx<uint32_t> b_vpl_count;
	btr::BufferMemoryEx<uint16_t> b_vpl_index;
	btr::BufferMemoryEx<uint32_t> b_vpl_duplicate;
	btr::BufferMemoryEx<uvec2> b_global_line_index;
	btr::BufferMemoryEx<u16vec2> b_global_line_data;
	btr::BufferMemoryEx<Emissive> v_emissive;
	btr::BufferMemoryEx<uint32_t> u_circle_mesh_count;
	btr::BufferMemoryEx<i16vec2> u_circle_mesh_vertex;
	btr::BufferMemoryEx<vk::DrawIndirectCommand> v_emissive_draw_command;
	btr::BufferMemoryEx<uint32_t> v_emissive_draw_count;


	GI2DRadiosityInfo m_info;

	vk::ImageCreateInfo m_image_info;
	vk::UniqueImage m_image;
	std::array<vk::UniqueImageView, Frame_Num> m_image_view;
	vk::UniqueImageView m_image_rtv;
	vk::UniqueDeviceMemory m_image_memory;
	vk::UniqueSampler m_image_sampler;
	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::UniqueRenderPass m_radiosity_pass;
	vk::UniqueFramebuffer m_radiosity_framebuffer;

	vk::UniqueRenderPass m_rendering_pass;
	vk::UniqueFramebuffer m_rendering_framebuffer;

	vk::UniqueRenderPass m_make_vpl_renderpass;
	vk::UniqueFramebuffer m_make_vpl_framebuffer;

	uvec2 m_radiosity_texture_size;

};

