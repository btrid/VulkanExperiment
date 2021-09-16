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
#include <btrlib/cModel.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/sCameraManager.h>

#include <btrlib/Context.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include <tinygltf/tiny_gltf.h>

namespace GLtoVK
{
	vk::Format toFormat(int gltf_type, int component_type)
	{
		switch (gltf_type)
		{
		case TINYGLTF_TYPE_SCALAR:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				return vk::Format::eR32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32Sfloat;
			}
		case TINYGLTF_TYPE_VEC2:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8G8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16G16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: 
				return vk::Format::eR32G32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32G32Sfloat;
			}
		case TINYGLTF_TYPE_VEC3:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8G8B8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16G16B16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				return vk::Format::eR32G32B32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32G32B32Sfloat;
			}
		case TINYGLTF_TYPE_VEC4:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8G8B8A8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16G16B16A16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				return vk::Format::eR32G32B32A32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32G32B32A32Sfloat;
			}
		}
		assert(false);
		return vk::Format::eUndefined;
	}
	
#define TINYGLTF_TYPE_VEC2 (2)
#define TINYGLTF_TYPE_VEC3 (3)
#define TINYGLTF_TYPE_VEC4 (4)

}

struct Context
{
	enum DSL
	{
		DSL_Model,
		DSL_Num,
	};

	std::shared_ptr<btr::Context> m_ctx;

	vk::UniqueDescriptorSetLayout m_DSL[DSL_Num];

	Context(std::shared_ptr<btr::Context>& ctx)
	{
		m_ctx = ctx;
		auto cmd = ctx->m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Model] = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}

		}

	}
};
struct Model
{
	struct Info
	{
		vec4 m_aabb_min;
		vec4 m_aabb_max;
		uint m_vertex_num;
		uint m_primitive_num;
		uvec3 m_voxel_reso;
	};
//	btr::BufferMemory b_vertex;
//	btr::BufferMemory b_normal;
//	btr::BufferMemory b_index;
//	btr::BufferMemoryEx<Info> u_info;
//	btr::BufferMemoryEx<vk::DrawIndexedIndirectCommand> b_draw_cmd;

	struct Mesh
	{
		std::vector<vk::VertexInputBindingDescription> vib_disc;
		std::vector<vk::VertexInputAttributeDescription> vid_disc;
	};
	std::vector<Mesh> m_meshes;
	std::vector<btr::BufferMemory> b_vertex;
	tinygltf::Model gltf_model;

	vk::UniqueDescriptorSet m_DS_Model;

	Info m_info;

	static std::shared_ptr<Model> LoadModel(Context& ctx, vk::CommandBuffer cmd, const std::string& filename)
	{
		tinygltf::Model gltf_model;
		{
			tinygltf::TinyGLTF loader;
			std::string err;
			std::string warn;

//			loader.
			bool res = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, filename);
			if (!warn.empty()) { std::cout << "WARN: " << warn << std::endl; }
			if (!err.empty()) { std::cout << "ERR: " << err << std::endl; }

			if (!res) std::cout << "Failed to load glTF: " << filename << std::endl;
			else std::cout << "Loaded glTF: " << filename << std::endl;
		}

		auto model = std::make_shared<Model>();
		std::vector<vk::DescriptorBufferInfo> info(gltf_model.buffers.size());
		uint buffersize_total = 0;
		model->b_vertex.resize(gltf_model.buffers.size());
		std::vector<btr::BufferMemory> staging(gltf_model.buffers.size());
		for (int i = 0; i < gltf_model.buffers.size(); i++)
		{
			model->b_vertex[i] = ctx.m_ctx->m_vertex_memory.allocateMemory(gltf_model.buffers[i].data.size());
			staging[i] = ctx.m_ctx->m_staging_memory.allocateMemory(gltf_model.buffers[i].data.size());
			memcpy_s(staging[i].getMappedPtr(), gltf_model.buffers[i].data.size(), gltf_model.buffers[i].data.data(), gltf_model.buffers[i].data.size());

			vk::BufferCopy copy = vk::BufferCopy().setSize(gltf_model.buffers[i].data.size()).setSrcOffset(staging[i].getInfo().offset).setDstOffset(model->b_vertex[i].getInfo().offset);
			cmd.copyBuffer(staging[i].getInfo().buffer, model->b_vertex[i].getInfo().buffer, copy);

			info[i].offset = info[std::max(i-1, 0)].offset+info[std::max(i-1, 0)].range;
			buffersize_total += gltf_model.buffers[i].data.size();
		}

		model->m_meshes.resize(gltf_model.meshes.size());
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			auto& gltf_mesh = gltf_model.meshes[mesh_index];
			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i) 
			{
				tinygltf::Primitive primitive = gltf_mesh.primitives[i];
				tinygltf::Accessor indexAccessor = gltf_model.accessors[primitive.indices];
				tinygltf::BufferView bufferview_index = gltf_model.bufferViews[indexAccessor.bufferView];

				for (auto& attrib : primitive.attributes) 
				{
					tinygltf::Accessor accessor = gltf_model.accessors[attrib.second];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					int size = 1;
					if (accessor.type != TINYGLTF_TYPE_SCALAR) 
					{
						size = accessor.type;
					}

					int vaa = -1;
					if (attrib.first.compare("POSITION") == 0) vaa = 0;
					if (attrib.first.compare("NORMAL") == 0) vaa = 1;
					if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
					if (vaa > -1) 
					{
						model->m_meshes[mesh_index].vib_disc.push_back(vk::VertexInputBindingDescription().setBinding(vaa).setInputRate(vk::VertexInputRate::eVertex).setStride(accessor.ByteStride(bufferview)));
						model->m_meshes[mesh_index].vid_disc.push_back(vk::VertexInputAttributeDescription().setBinding(vaa).setLocation(vaa).setFormat(GLtoVK::toFormat(size, accessor.componentType)).setOffset(accessor.byteOffset));
					}
					else
					{
						std::cout << "vaa missing: " << attrib.first << std::endl;
					}
				}

				if (gltf_model.textures.size() > 0) 
				{
// 					// fixme: Use material's baseColor
// 					tinygltf::Texture& tex = model.textures[0];
// 
// 					if (tex.source > -1) {
// 
// 						GLuint texid;
// 						glGenTextures(1, &texid);
// 
// 						tinygltf::Image& image = model.images[tex.source];
// 
// 						glBindTexture(GL_TEXTURE_2D, texid);
// 						glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
// 						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
// 						glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
// 						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
// 						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
// 
// 						GLenum format = GL_RGBA;
// 
// 						if (image.component == 1) {
// 							format = GL_RED;
// 						}
// 						else if (image.component == 2) {
// 							format = GL_RG;
// 						}
// 						else if (image.component == 3) {
// 							format = GL_RGB;
// 						}
// 						else {
// 							// ???
// 						}
// 
// 						GLenum type = GL_UNSIGNED_BYTE;
// 						if (image.bits == 8) {
// 							// ok
// 						}
// 						else if (image.bits == 16) {
// 							type = GL_UNSIGNED_SHORT;
// 						}
// 						else {
// 							// ???
// 						}
// 
// 						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0,
// 							format, type, &image.image.at(0));
// 					}
				}
			}

//			return vbos;
		}

// 		// bind models
// 		void bindModelNodes(std::map<int, GLuint> vbos, tinygltf::Model & model, tinygltf::Node & node) 
//		{
// 			if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
// 				bindMesh(vbos, model, model.meshes[node.mesh]);
// 			}
// 
// 			for (size_t i = 0; i < node.children.size(); i++) {
// 				assert((node.children[i] >= 0) && (node.children[i] < model.nodes.size()));
// 				bindModelNodes(vbos, model, model.nodes[node.children[i]]);
// 			}
// 		}
// 		GLuint bindModel(tinygltf::Model & model) 
// 		{
// 			std::map<int, GLuint> vbos;
// 			GLuint vao;
// 			glGenVertexArrays(1, &vao);
// 			glBindVertexArray(vao);
// 
// 			const tinygltf::Scene& scene = model.scenes[model.defaultScene];
// 			for (size_t i = 0; i < scene.nodes.size(); ++i) {
// 				assert((scene.nodes[i] >= 0) && (scene.nodes[i] < model.nodes.size()));
// 				bindModelNodes(vbos, model, model.nodes[scene.nodes[i]]);
// 			}
// 
// 			glBindVertexArray(0);
// 			// cleanup vbos
// 			for (size_t i = 0; i < vbos.size(); ++i) {
// 				glDeleteBuffers(1, &vbos[i]);
// 			}
// 
// 			return vao;
// 		}

// 		void drawMesh(tinygltf::Model & model, tinygltf::Mesh & mesh) {
// 			for (size_t i = 0; i < mesh.primitives.size(); ++i) {
// 				tinygltf::Primitive primitive = mesh.primitives[i];
// 				tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];
// 
// 				glDrawElements(primitive.mode, indexAccessor.count,
// 					indexAccessor.componentType,
// 					BUFFER_OFFSET(indexAccessor.byteOffset));
// 			}
// 		}
// 
// 		// recursively draw node and children nodes of model
// 		void drawModelNodes(tinygltf::Model & model, tinygltf::Node & node) {
// 			if ((node.mesh >= 0) && (node.mesh < model.meshes.size())) {
// 				drawMesh(model, model.meshes[node.mesh]);
// 			}
// 			for (size_t i = 0; i < node.children.size(); i++) {
// 				drawModelNodes(model, model.nodes[node.children[i]]);
// 			}
// 		}
// 		void drawModel(GLuint vao, tinygltf::Model & model) {
// 			glBindVertexArray(vao);
// 
// 			const tinygltf::Scene& scene = model.scenes[model.defaultScene];
// 			for (size_t i = 0; i < scene.nodes.size(); ++i) {
// 				drawModelNodes(model, model.nodes[scene.nodes[i]]);
// 			}
// 
// 			glBindVertexArray(0);
// 		}
// 
/*
		info.m_vertex_num = numVertex;
		auto cmd = ctx.m_ctx->m_cmd_pool->allocCmdTempolary(0);
		std::shared_ptr<Model> model = std::make_shared<Model>();
		{
			model->b_vertex = ctx.m_ctx->m_vertex_memory.allocateMemory(numVertex * sizeof(aiVector3D));
			model->b_normal = ctx.m_ctx->m_vertex_memory.allocateMemory(numVertex * sizeof(aiVector3D));
			model->b_index = ctx.m_ctx->m_vertex_memory.allocateMemory(numIndex * sizeof(uint32_t));

			std::array<vk::BufferCopy, 3> copy;
			copy[0].srcOffset = vertex.getInfo().offset;
			copy[0].dstOffset = model->b_vertex.getInfo().offset;
			copy[0].size = vertex.getInfo().range;
			copy[1].srcOffset = normal.getInfo().offset;
			copy[1].dstOffset = model->b_normal.getInfo().offset;
			copy[1].size = normal.getInfo().range;
			copy[2].srcOffset = index.getInfo().offset;
			copy[2].dstOffset = model->b_index.getInfo().offset;
			copy[2].size = index.getInfo().range;
			cmd.copyBuffer(vertex.getInfo().buffer, model->b_vertex.getInfo().buffer, copy);
		}

		{
			info.m_primitive_num = numIndex / 3;
			model->m_info = info;
			model->u_info = ctx.m_ctx->m_uniform_memory.allocateMemory<Info>(1);

			cmd.updateBuffer<Info>(model->u_info.getInfo().buffer, model->u_info.getInfo().offset, info);

			model->b_draw_cmd = ctx.m_ctx->m_storage_memory.allocateMemory<vk::DrawIndexedIndirectCommand>(1);
			cmd.updateBuffer<vk::DrawIndexedIndirectCommand>(model->b_draw_cmd.getInfo().buffer, model->b_draw_cmd.getInfo().offset, vk::DrawIndexedIndirectCommand(numIndex, 1, 0, 0, 0));
		}


		vk::BufferMemoryBarrier barrier[] = {
			model->b_vertex.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->b_normal.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->b_index.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			model->u_info.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead),
		};
		cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR | vk::PipelineStageFlagBits::eRayTracingShaderKHR, {}, {}, { array_size(barrier), barrier }, {});


		{
			vk::DescriptorSetLayout layouts[] =
			{
				ctx.m_DSL[Context::DSL_Model].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			model->m_DS_Model = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorBufferInfo uniforms[] =
			{
				model->u_info.getInfo(),
			};
			vk::DescriptorBufferInfo model_storages[] =
			{
				model->b_vertex.getInfo(),
				model->b_normal.getInfo(),
				model->b_index.getInfo(),
				model->b_draw_cmd.getInfo(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
				.setDescriptorCount(array_length(uniforms))
				.setPBufferInfo(uniforms)
				.setDstBinding(0)
				.setDstSet(model->m_DS_Model.get()),
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
				.setDescriptorCount(array_length(model_storages))
				.setPBufferInfo(model_storages)
				.setDstBinding(1)
				.setDstSet(model->m_DS_Model.get()),
			};
			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
*/
		model->gltf_model = std::move(gltf_model);
		return model;
	}
};

struct ModelRenderer
{
	enum
	{
		DSL_Renderer,
		DSL_Num,
	};
	enum
	{
		DS_Renderer,
		DS_Num,
	};

	enum
	{
		Pipeline_Render,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_Render,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	vk::UniqueRenderPass m_renderpass;
	vk::UniqueFramebuffer m_framebuffer;

	ModelRenderer(Context& ctx, RenderTarget& rt)
	{
//		auto cmd = ctx.m_ctx->m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
// 		{
// 			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
// 			vk::DescriptorSetLayoutBinding binding[] = {
// 				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eUniformBuffer, 1, stage),
// 			};
// 			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
// 			desc_layout_info.setBindingCount(array_length(binding));
// 			desc_layout_info.setPBindings(binding);
// 			m_DSL[DSL_Renderer] = ctx.m_ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
// 		}

		// descriptor set
		{
// 			vk::DescriptorSetLayout layouts[] =
// 			{
// 				m_DSL[DSL_Renderer].get(),
// 			};
// 			vk::DescriptorSetAllocateInfo desc_info;
// 			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
// 			desc_info.setDescriptorSetCount(array_length(layouts));
// 			desc_info.setPSetLayouts(layouts);
// 			m_DS[DS_Renderer] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
//
// 			vk::DescriptorBufferInfo uniforms[] =
// 			{
// 				u_info.getInfo(),
// 			};
// 			vk::DescriptorBufferInfo storages[] =
// 			{
// 				b_hashmap.getInfo(),
// 				b_hashmap_mask.getInfo(),
// 				b_interior_counter.getInfo(),
// 				b_leaf_counter.getInfo(),
// 				b_interior.getInfo(),
// 				b_leaf.getInfo(),
// 			};
// 
// 			vk::WriteDescriptorSet write[] =
// 			{
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
// 				.setDescriptorCount(array_length(uniforms))
// 				.setPBufferInfo(uniforms)
// 				.setDstBinding(0)
// 				.setDstSet(m_DS[DS_Renderer].get()),
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 				.setDescriptorCount(array_length(storages))
// 				.setPBufferInfo(storages)
// 				.setDstBinding(1)
// 				.setDstSet(m_DS[DS_Renderer].get()),
// 			};
// 			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
//					m_DSL[DSL_Renderer].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
//					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_Render] = ctx.m_ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// compute pipeline
		{
// 			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
// 			{
// 				{"Voxel_AllocateTopChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 				{"Voxel_AllocateMidChild.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 				{"Voxel_MakeHashMapMask.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 				{"Voxel_Rendering.comp.spv", vk::ShaderStageFlagBits::eCompute},
// 			};
// 			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
// 			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
// 			for (size_t i = 0; i < array_length(shader_param); i++)
// 			{
// 				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
// 				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
// 			}
// 
// 			vk::ComputePipelineCreateInfo compute_pipeline_info[] =
// 			{
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[0])
// 				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[1])
// 				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[2])
// 				.setLayout(m_PL[PipelineLayout_MakeVoxel].get()),
// 				vk::ComputePipelineCreateInfo()
// 				.setStage(shaderStages[3])
// 				.setLayout(m_PL[PipelineLayout_RenderVoxel].get()),
// 			};
// 			m_pipeline[Pipeline_MakeVoxelTopChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[0]).value;
// 			m_pipeline[Pipeline_MakeVoxelMidChild] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[1]).value;
// 			m_pipeline[Pipeline_MakeHashMapMask] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[2]).value;
// 			m_pipeline[Pipeline_RenderVoxel] = ctx.m_ctx->m_device.createComputePipelineUnique(vk::PipelineCache(), compute_pipeline_info[3]).value;
		}

		// graphics pipeline render
		{

			// レンダーパス
			{
				// sub pass
				vk::AttachmentReference color_ref[] =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(1);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(array_length(color_ref));
				subpass.setPColorAttachments(color_ref);
				subpass.setPDepthStencilAttachment(&depth_ref);

				vk::AttachmentDescription attach_description[] =
				{
					// color1
					vk::AttachmentDescription()
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					// depth
					vk::AttachmentDescription()
					.setFormat(rt.m_depth_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(array_length(attach_description));
				renderpass_info.setPAttachments(attach_description);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);
				m_renderpass = ctx.m_ctx->m_device.createRenderPassUnique(renderpass_info);
			}

			{
				vk::ImageView view[] = 
				{
					rt.m_view,
					rt.m_depth_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_renderpass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(rt.m_info.extent.width);
				framebuffer_info.setHeight(rt.m_info.extent.height);
				framebuffer_info.setLayers(1);
				m_framebuffer = ctx.m_ctx->m_device.createFramebufferUnique(framebuffer_info);
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"ModelRender.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"ModelRender.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport[] =
			{
				vk::Viewport(0.f, 0.f, rt.m_resolution.width, rt.m_resolution.height, 0.f, 1.f),
			};
			vk::Rect2D scissor[] =
			{
				vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution),
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);


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
			vk::VertexInputBindingDescription vi_binding[] =
			{
				vk::VertexInputBindingDescription().setBinding(0).setStride(12).setInputRate(vk::VertexInputRate::eVertex),
			};
			vk::VertexInputAttributeDescription vi_attrib[] =
			{
				vk::VertexInputAttributeDescription().setLocation(0).setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
			};
			vertex_input_info.vertexBindingDescriptionCount = array_size(vi_binding);
			vertex_input_info.pVertexBindingDescriptions = vi_binding;
			vertex_input_info.vertexAttributeDescriptionCount = array_size(vi_attrib);
			vertex_input_info.pVertexAttributeDescriptions = vi_attrib;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL[PipelineLayout_Render].get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline[Pipeline_Render] = ctx.m_ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}

	}

	void execute_Render(vk::CommandBuffer cmd, RenderTarget& rt, Model& model)
	{
		DebugLabel _label(cmd, __FUNCTION__);
		{
			vk::ImageMemoryBarrier image_barrier[1];
			image_barrier[0].setImage(rt.m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Render].get());
//		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { m_DS[DSL_Renderer].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
//		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 2, { rt.m_descriptor.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		auto& gltf_model = model.gltf_model;
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			auto& gltf_mesh = gltf_model.meshes[mesh_index];
			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
			{
				tinygltf::Primitive primitive = gltf_mesh.primitives[i];
				for (auto& attrib : primitive.attributes)
				{
					tinygltf::Accessor accessor = gltf_model.accessors[attrib.second];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					int size = 1;
					if (accessor.type != TINYGLTF_TYPE_SCALAR)
					{
						size = accessor.type;
					}

					int vaa = -1;
					if (attrib.first.compare("POSITION") == 0) vaa = 0;
					if (attrib.first.compare("NORMAL") == 0) vaa = 1;
					if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
					if (vaa > -1)
					{
						cmd.bindVertexBuffers(vaa, model.b_vertex[bufferview.buffer].getInfo().buffer, model.b_vertex[bufferview.buffer].getInfo().offset + bufferview.byteOffset);
					}
					else
					{
						std::cout << "vaa missing: " << attrib.first << std::endl;
					}
				}
				{
					tinygltf::Accessor accessor = gltf_model.accessors[primitive.indices];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					vk::IndexType indextype = vk::IndexType::eUint32;
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: indextype = vk::IndexType::eUint32; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indextype = vk::IndexType::eUint16; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: indextype = vk::IndexType::eUint8EXT; break;
					default: assert(false); break;
					}

					cmd.bindIndexBuffer(model.b_vertex[bufferview.buffer].getInfo().buffer, model.b_vertex[bufferview.buffer].getInfo().offset + bufferview.byteOffset, indextype);
					cmd.drawIndexed(accessor.count, 1, 0, 0, 0);
				}
			}
		}


		cmd.endRenderPass();

	}
};



int main()
{

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-100.f, 0.f, -100.f);
	camera->getData().m_target = vec3(0.f, 0.1f, 0.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	auto ctx = std::make_shared<Context>(context);

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	std::shared_ptr<Model> model;
	{
		auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);
		model = Model::LoadModel(*ctx, setup_cmd, btr::getResourceAppPath() + "pbr/DamagedHelmet.gltf");

	}

	app.setup();

	ModelRenderer renderer(*ctx, *app.m_window->getFrontBuffer());
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
					renderer.execute_Render(cmd, *render_target, *model);
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

