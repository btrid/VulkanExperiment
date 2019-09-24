#pragma once

#include <btrlib/Define.h>
#include <filesystem>
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include <tiny_gltf/tiny_gltf.h>

#include <btrlib/Context.h>

struct Attribute
{
	int32_t m_buffer_index;
	int32_t m_buffer_offset;
	int32_t m_buffer_count;
	uint32_t m_element_stride;
	uint32_t m_element_offset;
};
struct Primitive
{
	int32_t m_indexbuffer_index;
	int32_t m_primitive_topology;
	int32_t m_index_type;
	int32_t m_indexbuffer_offset;
	int32_t m_indexbuffer_count;

	std::vector<Attribute> m_vertex_attribute;

	int32_t m_material;


};
struct Mesh
{
	std::vector<Primitive> m_primitive;
};

struct VertexBuffer
{
	VkBuffer m_buffer;
	VmaAllocation m_allocation;
	VmaAllocator m_allocator;

	VertexBuffer()
		: m_buffer(VK_NULL_HANDLE)
		, m_allocation()
		, m_allocator()
	{
	}
	VertexBuffer(const std::shared_ptr<btr::Context>& context, const vk::BufferCreateInfo& info, const VmaAllocationCreateInfo& alloc_info)
	{
		vmaCreateBuffer(context->m_allocator, &(VkBufferCreateInfo)info, &alloc_info, &m_buffer, &m_allocation, nullptr);
		m_allocator = context->m_allocator;
	}

	~VertexBuffer()
	{
		if (m_buffer != VK_NULL_HANDLE)
		{
			vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
			m_buffer = VK_NULL_HANDLE;
		}
	}

	VertexBuffer(VertexBuffer&& rhv)noexcept
	{
		*this = std::move(rhv);
	}
	VertexBuffer& operator=(VertexBuffer&& rhv)noexcept
	{
		if (this != &rhv) 
		{
			this->~VertexBuffer();
			m_buffer = rhv.m_buffer;
			m_allocation = rhv.m_allocation;
			m_allocator = rhv.m_allocator;


			rhv.m_buffer = VK_NULL_HANDLE;
		}
		return *this;
	}

	VertexBuffer(const VertexBuffer&) = delete;
	VertexBuffer operator=(const VertexBuffer&) = delete;
};
struct Model 
{
	std::vector<btr::BufferMemory> m_buffer;
	std::vector<Mesh> m_mesh;
	Model(const std::shared_ptr<btr::Context>& context, const std::string &filename)
	{

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;
		const std::string ext = std::experimental::filesystem::path(filename).extension().generic_string();

		bool ret = false;
		if (ext.compare("glb") == 0) {
			// assume binary glTF.
			ret = loader.LoadBinaryFromFile(&model, &err, &warn, filename.c_str());
		}
		else {
			// assume ascii glTF.
			ret = loader.LoadASCIIFromFile(&model, &err, &warn, filename.c_str());
		}
		if (!warn.empty()) {
			printf("glTF parse warning: %s\n", warn.c_str());
		}
		if (!err.empty()) {
			printf("glTF parse error: %s\n", err.c_str());
		}
		if (!ret) {
			printf("Failed to load glTF: %s\n", filename.c_str());
		}


		// Iterate through all the meshes in the glTF file

		m_buffer.reserve(model.buffers.size());
		for (auto& gltf_buffer : model.buffers)
		{
			auto buffer = context->m_vertex_memory.allocateMemory(gltf_buffer.data.size());
			auto staging = context->m_staging_memory.allocateMemory(gltf_buffer.data.size(), true);
			memcpy(staging.getMappedPtr(), gltf_buffer.data.data(), gltf_buffer.data.size());

			vk::BufferCopy copy;
			copy.srcOffset = buffer.getInfo().offset;
			copy.dstOffset = staging.getInfo().offset;
			copy.size = gltf_buffer.data.size();

			auto cmd = context->m_cmd_pool->allocCmdTempolary(0);
			cmd.copyBuffer(buffer.getInfo().buffer, staging.getInfo().buffer, copy);
		}

		m_mesh.resize(model.meshes.size());
		for (size_t mesh_index = 0; mesh_index < model.meshes.size(); mesh_index++)
		{
			const auto& gltf_mesh = model.meshes[mesh_index];
			auto& mesh = m_mesh[mesh_index];
			mesh.m_primitive.resize(gltf_mesh.primitives.size());

			// For each primitive
			for (size_t primitive_index = 0; primitive_index < gltf_mesh.primitives.size(); primitive_index++)
			{
				const auto &gltf_primitive = gltf_mesh.primitives[primitive_index];
				auto &primitive = mesh.m_primitive[primitive_index];
				{
					primitive.m_indexbuffer_index = gltf_primitive.indices;
					primitive.m_primitive_topology = gltf_primitive.mode;

					const auto &accessor = model.accessors[gltf_primitive.indices];
					const auto &buffer_view = model.bufferViews[accessor.bufferView];
					primitive.m_index_type = accessor.componentType;
					primitive.m_indexbuffer_offset = buffer_view.byteOffset;
					primitive.m_indexbuffer_count = accessor.count;
				}

				primitive.m_vertex_attribute.resize(gltf_primitive.attributes.size());
				for (size_t attribute_i = 0; attribute_i < gltf_primitive.attributes.size(); attribute_i++)
				{
					const auto &attribute = gltf_primitive.attributes[attribute_i];
					const auto &accessor = model.accessors[attribute.second];
					const auto &buffer_view = model.bufferViews[accessor.bufferView];

					primitive.m_vertex_attribute[attribute_i].m_buffer_index = buffer_view.buffer;
					primitive.m_vertex_attribute[attribute_i].m_buffer_offset = buffer_view.byteOffset;
					primitive.m_vertex_attribute[attribute_i].m_buffer_count = accessor.count;
					primitive.m_vertex_attribute[attribute_i].m_element_offset = accessor.byteOffset;
					primitive.m_vertex_attribute[attribute_i].m_element_stride = accessor.ByteStride(buffer_view);
				}
			}
		}

// 		// Iterate through all texture declaration in glTF file
// 		for (const auto &gltfTexture : model.textures) 
// 		{
// 			std::cout << "Found texture!";
// 			Texture loadedTexture;
// 			const auto &image = model.images[gltfTexture.source];
// 			loadedTexture.components = image.component;
// 			loadedTexture.width = image.width;
// 			loadedTexture.height = image.height;
// 
// 			const auto size =
// 				image.component * image.width * image.height * sizeof(unsigned char);
// 			loadedTexture.image = new unsigned char[size];
// 			memcpy(loadedTexture.image, image.image.data(), size);
// 			textures->push_back(loadedTexture);
// 		}
// 		return ret;
	}

};