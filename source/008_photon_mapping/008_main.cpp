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
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>

#include <btrlib/Context.h>

#pragma comment(lib, "vulkan-1.lib")


struct Geometry
{
	static std::tuple<std::vector<glm::vec3>, std::vector<glm::uvec3>> GetPlane()
	{
		std::vector<glm::vec3> v(4);
		v[0] = { -0.5f, 0.f, -0.5f };
		v[1] = { -0.5f, 0.f, 0.5f };
		v[2] = { 0.5f, 0.f, -0.5f };
		v[3] = { 0.5f, 0.f, 0.5f };

		std::vector<glm::uvec3> i(2);
		i[0] = { 0, 1, 2 };
		i[1] = { 1, 3, 2 };

		return std::tie(v, i);
	}

	static std::tuple<std::vector<vec3>, std::vector<uvec3>> GetBox()
	{
		std::vector<vec3> box = 
		{
			{ -0.5f, 0.5f, 0.5f},
			{ 0.5f, 0.5f, 0.5f},
			{ -0.5f, -0.5f, 0.5f},
			{ 0.5f, -0.5f, 0.5f},
			{ -0.5f, 0.5f, -0.5f},
			{ 0.5f, 0.5f, -0.5f},
			{ -0.5f, -0.5f, -0.5f},
			{ 0.5f, -0.5f, -0.5f},
		};

		std::vector<uvec3> boxFace =
		{
			{ 0, 2, 3 },
			{ 0, 3, 1 },
			{ 0, 1, 5 },
			{ 0, 5, 4 },
			{ 6, 7, 3 },
			{ 6, 3, 2 },
			{ 0, 4, 6 },
			{ 0, 6, 2 },
			{ 3, 7, 5 },
			{ 3, 5, 1 },
			{ 5, 7, 6 },
			{ 5, 6, 4 },
		};
		return std::tie(box, boxFace);

	}

	static std::tuple<std::vector<vec3>, std::vector<uvec3>> getSphere()
	{
		// create 12 vertices of a icosahedron
		auto t = (1.f + glm::sqrt(5.f)) / 2.f;
		std::vector<vec3> vertex;

		vertex.push_back(normalize(vec3(-1, t, 0)));
		vertex.push_back(normalize(vec3(1, t, 0)));
		vertex.push_back(normalize(vec3(-1, -t, 0)));
		vertex.push_back(normalize(vec3(1, -t, 0)));

		vertex.push_back(normalize(vec3(0, -1, t)));
		vertex.push_back(normalize(vec3(0, 1, t)));
		vertex.push_back(normalize(vec3(0, -1, -t)));
		vertex.push_back(normalize(vec3(0, 1, -t)));

		vertex.push_back(normalize(vec3(t, 0, -1)));
		vertex.push_back(normalize(vec3(t, 0, 1)));
		vertex.push_back(normalize(vec3(-t, 0, -1)));
		vertex.push_back(normalize(vec3(-t, 0, 1)));


		// create 20 triangles of the icosahedron
		std::vector<uvec3> face;

		// 5 faces around point 0
		face.emplace_back(0, 11, 5);
		face.emplace_back(0, 5, 1);
		face.emplace_back(0, 1, 7);
		face.emplace_back(0, 7, 10);
		face.emplace_back(0, 10, 11);

		// 5 adjacent faces 
		face.emplace_back(1, 5, 9);
		face.emplace_back(5, 11, 4);
		face.emplace_back(11, 10, 2);
		face.emplace_back(10, 7, 6);
		face.emplace_back(7, 1, 8);

		// 5 faces around point 3
		face.emplace_back(3, 9, 4);
		face.emplace_back(3, 4, 2);
		face.emplace_back(3, 2, 6);
		face.emplace_back(3, 6, 8);
		face.emplace_back(3, 8, 9);

		// 5 adjacent faces 
		face.emplace_back(4, 9, 5);
		face.emplace_back(2, 4, 11);
		face.emplace_back(6, 2, 10);
		face.emplace_back(8, 6, 7);
		face.emplace_back(9, 8, 1);

		return std::tie(vertex, face);
	}
};
struct TriangleList 
{
};

struct PhotonMapping
{

	enum Shader
	{
		Shader_MakeBV_VS,
		Shader_MakeBV_GS,
		Shader_MakeBV_FS,
		Shader_Emit,
		Shader_Bounce,
		Shader_Render,
		Shader_Num,
	};

	enum PipelineLayout
	{
		PipelineLayout_PM,
		PipelineLayout_Render,
		PipelineLayout_Num,
	};
	enum Pipeline
	{
		Pipeline_MakeBV,
		Pipeline_Emit,
		Pipeline_Bounce,
		Pipeline_Render,
		Pipeline_Num,
	};

	struct Vertex {
		vec3 Position;
		int32_t MaterialIndex;
		vec2 m_texcoord;
		vec2 m_texcoord1;
	};

	struct TriangleMesh
	{
		Vertex a, b, c;
	};

	struct Material
	{
		vec4 	Diffuse;
		vec4 	Emissive;
		float	m_diffuse;
		float	m_specular; //!< 光沢
		float	m_rough; //!< ざらざら
		float	m_metalic;

		uint32_t	is_emissive;
		uint32_t	m_diffuse_tex;
		uint32_t	m_ambient_tex;
		uint32_t	m_specular_tex;
		uint32_t	m_emissive_tex;

	};

	struct DrawCommand// : public DrawElementsIndirectCommand
	{
		uint32_t count;
		uint32_t instanceCount;
		uint32_t firstIndex;
		uint32_t baseVertex;

		uint32_t baseInstance;
		uint32_t is_emissive;
		uint32_t _p[2];

		vec4 aabb_max;
		vec4 aabb_min;
	};

	struct TriangleLL
	{
		uint32_t next;
		uint32_t drawID;
		uint32_t instanceID;
		uint32_t _p;
		uint32_t index[3];
		uint32_t _p2;
	};

	struct PMInfo
	{
		uvec4 num0;
		vec4 cell_size;
		vec4 area_min;
		vec4 area_max;
	};

	struct Photon
	{
		vec3 Position;
		uint32_t Dir_16_16; //!< 極座標[theta, phi]
		vec3 Power; //!< flux 放射束
	//	uint Power10_11_11; //!< @todo
		uint TriangleLLIndex; //!< hitした三角形のインデックス
	};


	PhotonMapping(const std::shared_ptr<btr::Context>& context)
	{
		PMInfo info;
		info.area_max = vec4(1000.f);
		info.area_min = vec4(0.f);
		info.num0 = uvec4(256);
		info.num0.w = info.num0.x*info.num0.y*info.num0.z;
		info.cell_size = (info.area_max - info.area_min) / info.cell_size;

		{
			// sub pass
			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(0);
			subpass.setPColorAttachments(nullptr);

			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);

			m_make_bv_renderpass = context->m_device.createRenderPassUnique(renderpass_info);

			{
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_make_bv_renderpass.get());
				framebuffer_info.setWidth(info.num0.x);
				framebuffer_info.setHeight(info.num0.y);
				framebuffer_info.setLayers(1);

				m_make_bv_framebuffer = context->m_device.createFramebufferUnique(framebuffer_info);
			}
		}

		{

			{
				auto stage = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;
				auto storage = vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setDescriptorCount(1)
					.setBinding(10);

				vk::DescriptorSetLayoutBinding binding[] = {
					vk::DescriptorSetLayoutBinding()
					.setStageFlags(stage)
					.setDescriptorType(vk::DescriptorType::eUniformBuffer)
					.setDescriptorCount(1)
					.setBinding(0),
					storage.setBinding(10),
					storage.setBinding(11),
					storage.setBinding(12),
					storage.setBinding(13),
					storage.setBinding(20),
					storage.setBinding(21),
					storage.setBinding(22),
					storage.setBinding(23),
					storage.setBinding(30),
					storage.setBinding(31),
					storage.setBinding(32),
					storage.setBinding(33),
					storage.setBinding(34),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_descriptor_set_layout = context->m_device.createDescriptorSetLayoutUnique(desc_layout_info);

			}

			{
				const char* name[] =
				{
					"PM_MakeVoxel.vert.spv",
					"PM_MakeVoxel.geom.spv",
					"PM_MakeVoxel.frag.spv",
					"PhotonMapping.comp.spv",
					"PhotonMappingBounce.comp.spv",
					"PhotonRendering.comp.spv",
				};
				static_assert(array_length(name) == Shader_Num, "not equal shader num");

				std::string path = btr::getResourceShaderPath();
				for (size_t i = 0; i < array_length(name); i++) {
					m_shader[i] = loadShaderUnique(context->m_device, path + name[i]);
				}
			}

			// pipeline layout
			{
				vk::DescriptorSetLayout layouts[] = {
					m_descriptor_set_layout.get(),
				};

				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				{
					vk::PushConstantRange constants[] = {
						vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setSize(4).setOffset(0),
					};
					pipeline_layout_info.setPPushConstantRanges(constants);
					pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
					m_pipeline_layout[PipelineLayout_PM] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
				}
				{
					vk::PushConstantRange constants[] = {
						vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setSize(16).setOffset(0),
						vk::PushConstantRange().setStageFlags(vk::ShaderStageFlagBits::eCompute).setSize(16).setOffset(16),
					};
					pipeline_layout_info.setPPushConstantRanges(constants);
					pipeline_layout_info.setPushConstantRangeCount(array_length(constants));
					m_pipeline_layout[PipelineLayout_Render] = context->m_device.createPipelineLayoutUnique(pipeline_layout_info);
				}

			}

			// pipeline
			{
				std::array<vk::PipelineShaderStageCreateInfo, 3> shader_info;
				shader_info[0].setModule(m_shader[Shader_Emit].get());
				shader_info[0].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[0].setPName("main");
				shader_info[1].setModule(m_shader[Shader_Bounce].get());
				shader_info[1].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[1].setPName("main");
				shader_info[2].setModule(m_shader[Shader_Render].get());
				shader_info[2].setStage(vk::ShaderStageFlagBits::eCompute);
				shader_info[2].setPName("main");
				std::vector<vk::ComputePipelineCreateInfo> compute_pipeline_info =
				{
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[0])
					.setLayout(m_pipeline_layout[PipelineLayout_PM].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[1])
					.setLayout(m_pipeline_layout[PipelineLayout_PM].get()),
					vk::ComputePipelineCreateInfo()
					.setStage(shader_info[2])
					.setLayout(m_pipeline_layout[PipelineLayout_Render].get()),
				};
				auto compute_pipeline = context->m_device.createComputePipelinesUnique(vk::PipelineCache(), compute_pipeline_info);
				m_pipeline[Pipeline_Emit] = std::move(compute_pipeline[0]);
				m_pipeline[Pipeline_Bounce] = std::move(compute_pipeline[1]);
				m_pipeline[Pipeline_Render] = std::move(compute_pipeline[2]);
			}

			// graphics pipeline
			{
				// assembly
				vk::PipelineInputAssemblyStateCreateInfo assembly_info = vk::PipelineInputAssemblyStateCreateInfo()
					.setPrimitiveRestartEnable(VK_FALSE)
					.setTopology(vk::PrimitiveTopology::eTriangleList);

				// viewport
				vk::Extent3D size;
				size.setWidth(info.num0.x);
				size.setHeight(info.num0.y);
				size.setDepth(info.num0.z);
				vk::Viewport viewports[] = {
					vk::Viewport(0.f, 0.f, (float)size.depth, (float)size.height, 0.f, 1.f),
					vk::Viewport(0.f, 0.f, (float)size.width, (float)size.depth, 0.f, 1.f),
					vk::Viewport(0.f, 0.f, (float)size.width, (float)size.height, 0.f, 1.f),
				};
				std::vector<vk::Rect2D> scissor = {
					vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.depth, size.height)),
					vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.depth)),
					vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(size.width, size.height)),
				};
				vk::PipelineViewportStateCreateInfo viewport_info;
				viewport_info.setViewportCount(array_length(viewports));
				viewport_info.setPViewports(viewports);
				viewport_info.setScissorCount((uint32_t)scissor.size());
				viewport_info.setPScissors(scissor.data());

				// ラスタライズ
				vk::PipelineRasterizationStateCreateInfo rasterization_info;
				rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
				rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
				rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
				rasterization_info.setLineWidth(1.f);
				// サンプリング
				vk::PipelineMultisampleStateCreateInfo sample_info;
				sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

				// デプスステンシル
				vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
				depth_stencil_info.setDepthTestEnable(VK_FALSE);
				depth_stencil_info.setDepthWriteEnable(VK_FALSE);
				depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
				depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
				depth_stencil_info.setStencilTestEnable(VK_FALSE);

				// ブレンド
				std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				};
				vk::PipelineColorBlendStateCreateInfo blend_info;
				blend_info.setAttachmentCount(blend_state.size());
				blend_info.setPAttachments(blend_state.data());

				std::vector<vk::VertexInputBindingDescription> vertex_input_binding =
				{
					vk::VertexInputBindingDescription()
					.setBinding(0)
					.setInputRate(vk::VertexInputRate::eVertex)
					.setStride(sizeof(Vertex))
				};

				std::vector<vk::VertexInputAttributeDescription> vertex_input_attribute =
				{
					// pos
					vk::VertexInputAttributeDescription()
					.setBinding(0)
					.setLocation(0)
					.setFormat(vk::Format::eR32G32B32Sfloat)
					.setOffset(0),
				};
				vk::PipelineVertexInputStateCreateInfo vertex_input_info;
				vertex_input_info.setVertexBindingDescriptionCount((uint32_t)vertex_input_binding.size());
				vertex_input_info.setPVertexBindingDescriptions(vertex_input_binding.data());
				vertex_input_info.setVertexAttributeDescriptionCount((uint32_t)vertex_input_attribute.size());
				vertex_input_info.setPVertexAttributeDescriptions(vertex_input_attribute.data());

				std::array<vk::PipelineShaderStageCreateInfo, 3> stage_info =
				{
					vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, m_shader[Shader_MakeBV_VS].get(), "main"),
					vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, m_shader[Shader_MakeBV_GS].get(), "main"),
					vk::PipelineShaderStageCreateInfo({}, vk::ShaderStageFlagBits::eVertex, m_shader[Shader_MakeBV_FS].get(), "main"),
				};
				std::vector<vk::GraphicsPipelineCreateInfo> graphics_pipeline_info =
				{
					vk::GraphicsPipelineCreateInfo()
					.setStageCount((uint32_t)stage_info.size())
					.setPStages(stage_info.data())
					.setPVertexInputState(&vertex_input_info)
					.setPInputAssemblyState(&assembly_info)
					.setPViewportState(&viewport_info)
					.setPRasterizationState(&rasterization_info)
					.setPMultisampleState(&sample_info)
					.setLayout(m_pipeline_layout[Pipeline_MakeBV].get())
					.setRenderPass(m_make_bv_renderpass.get())
					.setPDepthStencilState(&depth_stencil_info)
					.setPColorBlendState(&blend_info),
				};
				m_pipeline[Pipeline_MakeBV] = std::move(context->m_device.createGraphicsPipelinesUnique(vk::PipelineCache(), graphics_pipeline_info)[0]);

			}
		}

		u_pm_info = context->m_uniform_memory.allocateMemory<PMInfo>({ 1, {} });
		b_cmd = context->m_storage_memory.allocateMemory<DrawCommand>({ 100, {} });
		b_vertex = context->m_storage_memory.allocateMemory<Vertex>({ 10000, {} });
		b_element = context->m_storage_memory.allocateMemory<uvec3>({ 10000, {} });
		b_material = context->m_storage_memory.allocateMemory<Material>({ 100, {} });
		b_triangle_count = context->m_storage_memory.allocateMemory<uvec4>({ 1, {} });
		b_triangle_LL_head = context->m_storage_memory.allocateMemory<uint32_t>({ info.num0.w, {} });
		b_triangle_LL = context->m_storage_memory.allocateMemory<TriangleLL>({ 10000, {} });
		b_triangle_hierarchy = context->m_storage_memory.allocateMemory<uint64_t>({ info.num0.w, {} });
		bPhoton = context->m_storage_memory.allocateMemory<Photon>({ 100000, {} });
		b_photon_counter = context->m_storage_memory.allocateMemory<uvec4>({ 1, {} });
		bPhotonLLHead = context->m_storage_memory.allocateMemory<uint32_t>({ info.num0.w, {} });
		bPhotonLL = context->m_storage_memory.allocateMemory<uint32_t>({ 100000, {} });

		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
		cmd.updateBuffer<PMInfo>(u_pm_info.getInfo().buffer, u_pm_info.getInfo().offset, info);

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
				u_pm_info.getInfo(),
			};
			vk::DescriptorBufferInfo geometrys[] = {
				b_cmd.getInfo(),
				b_vertex.getInfo(),
				b_element.getInfo(),
				b_material.getInfo(),
			};
			vk::DescriptorBufferInfo triangles[] = {
				b_triangle_count.getInfo(),
				b_triangle_LL_head.getInfo(),
				b_triangle_LL.getInfo(),
				b_triangle_hierarchy.getInfo(),
			};
			vk::DescriptorBufferInfo photons[] = {
				bPhoton.getInfo(),
				b_photon_counter.getInfo(),
				bPhotonLLHead.getInfo(),
				bPhotonLL.getInfo(),
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
				.setDescriptorCount(array_length(geometrys))
				.setPBufferInfo(geometrys)
				.setDstBinding(10)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(triangles))
				.setPBufferInfo(triangles)
				.setDstBinding(20)
				.setDstSet(m_descriptor_set.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(photons))
				.setPBufferInfo(photons)
				.setDstBinding(30)
				.setDstSet(m_descriptor_set.get()),
			};
			context->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
		}
	}

	void execute(vk::CommandBuffer cmd)
	{
		{
			vk::BufferMemoryBarrier to_write[] = {
				b_triangle_count.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				b_triangle_LL_head.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
				b_photon_counter.makeMemoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, {},
				0, nullptr, array_length(to_write), to_write, 0, nullptr);

			cmd.fillBuffer(b_triangle_count.getInfo().buffer, b_triangle_count.getInfo().offset, b_triangle_count.getInfo().range, 0);
			cmd.fillBuffer(b_triangle_LL_head.getInfo().buffer, b_triangle_LL_head.getInfo().offset, b_triangle_LL_head.getInfo().range, -1);
			cmd.fillBuffer(b_photon_counter.getInfo().buffer, b_photon_counter.getInfo().offset, b_photon_counter.getInfo().range, 0);

		}

		{
			// Make Bounding Volume
			vk::BufferMemoryBarrier to_read[] = {
				b_triangle_count.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
				b_triangle_LL_head.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {},
				0, nullptr, array_length(to_read), to_read, 0, nullptr);

			cmd.bindPipeline(vk::PipelineBindPoint::eCompute, m_pipeline[Pipeline_MakeBV].get());

// 			for (int i = 0; i < m_m)
// 			{
// 			}
// 			auto num = app::calcDipatchGroups(uvec3(Particle_Num, 1, 1), uvec3(1024, 1, 1));
// 			cmd.dispatch(num.x, num.y, num.z);

		}

		cmd.bindDescriptorSets(vk::PipelineBindPoint::eCompute, m_pipeline_layout[PipelineLayout_PM].get(), 0, m_descriptor_set.get(), {});


	}

	btr::BufferMemoryEx<PMInfo> u_pm_info;
	btr::BufferMemoryEx<DrawCommand> b_cmd;
	btr::BufferMemoryEx<Vertex> b_vertex;
	btr::BufferMemoryEx<uvec3> b_element;
	btr::BufferMemoryEx<Material> b_material;
	btr::BufferMemoryEx<uvec4> b_triangle_count;
	btr::BufferMemoryEx<uint32_t> b_triangle_LL_head;
	btr::BufferMemoryEx<TriangleLL> b_triangle_LL;
	btr::BufferMemoryEx<uint64_t> b_triangle_hierarchy;
	btr::BufferMemoryEx<Photon> bPhoton;
	btr::BufferMemoryEx<uvec4> b_photon_counter;
	btr::BufferMemoryEx<uint32_t> bPhotonLLHead;
	btr::BufferMemoryEx<uint32_t> bPhotonLL;

	vk::UniqueDescriptorSetLayout m_descriptor_set_layout;
	vk::UniqueDescriptorSet m_descriptor_set;

	vk::DescriptorSet getDescriptorSet()const { return m_descriptor_set.get(); }
	vk::DescriptorSetLayout getDescriptorSetLayout()const { return m_descriptor_set_layout.get(); }

	std::array<vk::UniqueShaderModule, Shader_Num> m_shader;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_pipeline_layout;
	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;

	vk::UniqueRenderPass m_make_bv_renderpass;
	vk::UniqueFramebuffer m_make_bv_framebuffer;
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

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	ClearPipeline clear_pipeline(context, app.m_window->getFrontBuffer());
	PresentPipeline present_pipeline(context, app.m_window->getFrontBuffer(), app.m_window->getSwapchain());

	PhotonMapping pm(context);

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
//			pm.execute();
			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fs\n", time.getElapsedTimeAsSeconds());
	}

	return 0;
}

