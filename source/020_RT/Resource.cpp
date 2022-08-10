#include <020_RT/Resource.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include <tinygltf/tiny_gltf.h>

#include <gli/gli/gli.hpp>

#include <020_RT/nvmeshlet_array.hpp>

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

	vk::Format toFormat(int gltf_type, int component_num, int bits)
	{
		switch (component_num)
		{
		case 1:
			switch (bits)
			{
			case 8: return vk::Format::eR8Unorm;
			case 16: return vk::Format::eR16Unorm;
			case 32: return vk::Format::eR32Sfloat;
			}
		case 2:
			switch (bits)
			{
			case 8: return vk::Format::eR8G8Unorm;
			case 16: return vk::Format::eR16G16Unorm;
			case 32: return vk::Format::eR32G32Sfloat;
			}
		case 3:
			switch (bits)
			{
			case 8: return vk::Format::eR8G8B8Unorm;
			case 16: return vk::Format::eR16G16B16Unorm;
			case 32: return vk::Format::eR32G32B32Sfloat;
			}
		case 4:
			switch (bits)
			{
			case 8: return vk::Format::eR8G8B8A8Unorm;
			case 16: return vk::Format::eR16G16B16A16Unorm;
			case 32: return vk::Format::eR32G32B32A32Sfloat;
			}
		}
		assert(false);
		return vk::Format::eUndefined;
	}

	vk::Format toFormat(gli::format f)
	{
		// 間違ってる可能性はある
		return (vk::Format)f;
	}
}


Resource::Resource(btr::Context& ctx)
{
	b_models = ctx.m_vertex_memory.allocateMemory<uint64_t>(1024);
	{
		std::array<vk::DescriptorPoolSize, 2> pool_size;
		pool_size[0].setType(vk::DescriptorType::eStorageBuffer);
		pool_size[0].setDescriptorCount(array_size(m_buffer_info));
		pool_size[1].setType(vk::DescriptorType::eCombinedImageSampler);
		pool_size[1].setDescriptorCount(array_size(m_image_info));

		vk::DescriptorPoolCreateInfo pool_info;
		pool_info.setPoolSizeCount(array_length(pool_size));
		pool_info.setPPoolSizes(pool_size.data());
		pool_info.setMaxSets(1);
		pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind;
		m_descriptor_pool = ctx.m_device.createDescriptorPoolUnique(pool_info);

	}

	{
		auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eTaskNV | vk::ShaderStageFlagBits::eMeshNV;
		vk::DescriptorSetLayoutBinding binding[] =
		{
			vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageBuffer, 1, stage),
			vk::DescriptorSetLayoutBinding(100, vk::DescriptorType::eCombinedImageSampler, 1024, stage),
		};
		vk::DescriptorSetLayoutCreateInfo desc_layout_info;
		desc_layout_info.setBindingCount(array_length(binding));
		desc_layout_info.setPBindings(binding);
		m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
	}

	{
		m_buffer_info = {
			b_models.getInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
			ctx.m_vertex_memory.getDummyInfo(),
		};
		m_image_info.fill(vk::DescriptorImageInfo{ sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal });

		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS_ModelResource = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

		}

#if defined(USE_TEMPLATE) // updateDescriptorSetWithTemplateがバグってる
		{
			vk::DescriptorUpdateTemplateEntry dutEntry[2];
			dutEntry[0].setDstBinding(0).setDstArrayElement(0).setDescriptorCount(array_size(m_buffer_info)).setDescriptorType(vk::DescriptorType::eStorageBuffer).setOffset(offsetof(Resource, m_buffer_info)).setStride(sizeof(VkDescriptorBufferInfo));
			dutEntry[1].setDstBinding(100).setDstArrayElement(0).setDescriptorCount(array_size(m_image_info)).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setOffset(offsetof(Resource, m_image_info)).setStride(sizeof(VkDescriptorImageInfo));

			vk::DescriptorUpdateTemplateCreateInfo dutCI;
			dutCI.setTemplateType(vk::DescriptorUpdateTemplateType::eDescriptorSet);
			dutCI.descriptorSetLayout = m_DSL.get();
			dutCI.setDescriptorUpdateEntries(dutEntry);
			m_Texture_DUP = ctx.m_device.createDescriptorUpdateTemplateUnique(dutCI);
//			ctx.m_device.updateDescriptorSetWithTemplate(*m_DS_ModelResource, *m_Texture_DUP, this);
		}
#else
		vk::WriteDescriptorSet write[] =
		{
			vk::WriteDescriptorSet().setDstSet(*m_DS_ModelResource).setDstBinding(0).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(m_buffer_info),
			vk::WriteDescriptorSet().setDstSet(*m_DS_ModelResource).setDstBinding(100).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setImageInfo(m_image_info),
		};
		ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

#endif
	}
}

std::shared_ptr<gltf::gltfResource> Resource::LoadScene(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename)
{
	std::shared_ptr<gltf::gltfResource> resource;
	if (m_gltf_manager.getOrCreate(resource, filename))
	{
		return resource;
	}

	tinygltf::Model gltf_model;
	{
		tinygltf::TinyGLTF loader;
		std::string err;
		std::string warn;

		// loader.
		bool res = loader.LoadBinaryFromFile(&gltf_model, &err, &warn, filename);
		if (!res) res = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, filename);

		if (!warn.empty()) { std::cout << "WARN: " << warn << std::endl; }
		if (!err.empty()) { std::cout << "ERR: " << err << std::endl; }

		if (!res) std::cout << "Failed to load glTF: " << filename << std::endl;
		else std::cout << "Loaded glTF: " << filename << std::endl;
	}

	// buffer
	{
		resource->b_buffer.resize(gltf_model.buffers.size());
		std::vector<btr::BufferMemory> staging(gltf_model.buffers.size());
		for (int i = 0; i < gltf_model.buffers.size(); i++)
		{
			resource->b_buffer[i] = ctx.m_vertex_memory.allocateMemory(gltf_model.buffers[i].data.size());
			staging[i] = ctx.m_staging_memory.allocateMemory(gltf_model.buffers[i].data.size());
			memcpy_s(staging[i].getMappedPtr(), gltf_model.buffers[i].data.size(), gltf_model.buffers[i].data.data(), gltf_model.buffers[i].data.size());

			vk::BufferCopy copy = vk::BufferCopy().setSize(gltf_model.buffers[i].data.size()).setSrcOffset(staging[i].getInfo().offset).setDstOffset(resource->b_buffer[i].getInfo().offset);
			cmd.copyBuffer(staging[i].getInfo().buffer, resource->b_buffer[i].getInfo().buffer, copy);

			vk::BufferMemoryBarrier barrier[] = {
				resource->b_buffer[i].makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eAccelerationStructureReadKHR),
			};
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});

		}
	}

	// image 
	{
		resource->t_image.resize(gltf_model.images.size());
		for (size_t i = 0; i < gltf_model.images.size(); i++)
		{
			tinygltf::Texture& tex = gltf_model.textures[i];
			tinygltf::Image& image = gltf_model.images[i];

			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = GLtoVK::toFormat(image.pixel_type, image.component, image.bits);
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eLinear;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = vk::Extent3D(image.width, image.height, 1);

			resource->t_image[i] = LoadTexture(ctx, cmd, image.uri, image_info, image.image);
		}
	}
	// material
	{
		std::vector<gltf::Material> material(gltf_model.materials.size());
		for (size_t i = 0; i < gltf_model.materials.size(); i++)
		{
			auto gltf_material = gltf_model.materials[i];
			gltf::Material& m = material[i];

			m.m_basecolor_factor.x = (float)gltf_material.pbrMetallicRoughness.baseColorFactor[0];
			m.m_basecolor_factor.y = (float)gltf_material.pbrMetallicRoughness.baseColorFactor[1];
			m.m_basecolor_factor.z = (float)gltf_material.pbrMetallicRoughness.baseColorFactor[2];
			m.m_basecolor_factor.w = (float)gltf_material.pbrMetallicRoughness.baseColorFactor[3];
			m.m_metallic_factor = (float)gltf_material.pbrMetallicRoughness.metallicFactor;
			m.m_roughness_factor = (float)gltf_material.pbrMetallicRoughness.roughnessFactor;
			m.m_emissive_factor.x = (float)gltf_material.emissiveFactor[0];
			m.m_emissive_factor.y = (float)gltf_material.emissiveFactor[1];
			m.m_emissive_factor.z = (float)gltf_material.emissiveFactor[2];

// 			m.m_is_emissive_material = false;
// 			if (dot(m.m_emissive_factor, vec3(1.f)) > 0.f)
// 			{
// 				m.m_is_emissive_material = true;
// 			}

			m.TexID_Base = -1;
			if (gltf_material.pbrMetallicRoughness.baseColorTexture.index >= 0)
			{
				m.TexID_Base = resource->t_image[gltf_material.pbrMetallicRoughness.baseColorTexture.index]->m_block;
			}
			m.TexID_MR = -1;
			if (gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
			{
				m.TexID_MR = resource->t_image[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index]->m_block;
			}
			m.TexID_Normal = -1;
			if (gltf_material.normalTexture.index >= 0)
			{
				m.TexID_Normal = resource->t_image[gltf_material.normalTexture.index]->m_block;
			}
			m.TexID_AO = -1;
			if (gltf_material.occlusionTexture.index >= 0)
			{
				m.TexID_AO = resource->t_image[gltf_material.occlusionTexture.index]->m_block;
			}
			m.TexID_Emissive = -1;
			if (gltf_material.emissiveTexture.index >= 0)
			{
				m.TexID_Emissive = resource->t_image[gltf_material.emissiveTexture.index]->m_block;
			}
		}
		resource->b_material = ctx.m_vertex_memory.allocateMemory<gltf::Material>(material.size());
		cmd.updateBuffer<gltf::Material>(resource->b_material.getInfo().buffer, resource->b_material.getInfo().offset, material);
		resource->m_material = std::move(material);
	}


	// mesh
	resource->m_mesh.resize(gltf_model.meshes.size());
	for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
	{
		auto& gltf_mesh = gltf_model.meshes[mesh_index];
		auto& mesh = resource->m_mesh[mesh_index];
		mesh.m_primitive.resize(gltf_mesh.primitives.size());
		mesh.m_MeshletGeometry.resize(gltf_mesh.primitives.size());
		mesh.b_MeshletDesc.resize(gltf_mesh.primitives.size());;
		mesh.b_MeshletPack.resize(gltf_mesh.primitives.size());
		mesh.b_primitive = ctx.m_vertex_memory.allocateMemory<Primitive>(gltf_mesh.primitives.size());
		for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
		{
			tinygltf::Primitive& gltf_primitive = gltf_mesh.primitives[i];
			Primitive& primitive = mesh.m_primitive[i];

			// index
			tinygltf::Accessor& indexAccessor = gltf_model.accessors[gltf_primitive.indices];
			tinygltf::BufferView& bufferview_index = gltf_model.bufferViews[indexAccessor.bufferView];
			primitive.IndexAddress = resource->b_buffer[bufferview_index.buffer].getDeviceAddress() + bufferview_index.byteOffset;

			// vertex
			for (auto& attrib : gltf_primitive.attributes)
			{
				tinygltf::Accessor& accessor = gltf_model.accessors[attrib.second];
				tinygltf::BufferView& bufferview = gltf_model.bufferViews[accessor.bufferView];
				if (attrib.first.compare("POSITION") == 0)
				{
					primitive.VertexAddress = resource->b_buffer[bufferview.buffer].getDeviceAddress() + bufferview.byteOffset;
				}
				else if (attrib.first.compare("NORMAL") == 0)
				{
					primitive.NormalAddress = resource->b_buffer[bufferview.buffer].getDeviceAddress() + bufferview.byteOffset;
				}
				else if (attrib.first.compare("TEXCOORD_0") == 0)
				{
					primitive.TexcoordAddress = resource->b_buffer[bufferview.buffer].getDeviceAddress() + bufferview.byteOffset;
				}
				else if (attrib.first.compare("TANGENT") == 0)
				{
					primitive.TangentAddress = resource->b_buffer[bufferview.buffer].getDeviceAddress() + bufferview.byteOffset;
				}
				else
				{
					assert(false);
				}
			}
			primitive.PrimitiveAddress = mesh.b_primitive.getDeviceAddress() + i * sizeof(Primitive);
			primitive.MaterialAddress = resource->b_material.getDeviceAddress() + gltf_primitive.material * sizeof(gltf::Material);
//			primitive.m_is_emissive = resource->m_material[gltf_primitive.material].m_is_emissive_material;

			// meshlet
			NVMeshlet::PackBasicBuilder meshletBuilder;
			meshletBuilder.setup(64, 126, true);

			NVMeshlet::PackBasicBuilder::MeshletGeometry& meshlet = mesh.m_MeshletGeometry[i];
			auto vertex_attrib = gltf_primitive.attributes.find("POSITION");
			const tinygltf::Accessor& vertex_accessor = gltf_model.accessors[vertex_attrib->second];
			const tinygltf::BufferView& vertex_bufferview = gltf_model.bufferViews[vertex_accessor.bufferView];

			primitive.m_aabb_min = vec4(vertex_accessor.minValues[0], vertex_accessor.minValues[1], vertex_accessor.minValues[2], 0.f);
			primitive.m_aabb_max = vec4(vertex_accessor.maxValues[0], vertex_accessor.maxValues[1], vertex_accessor.maxValues[2], 0.f);
			auto* indices = (gltf_model.buffers[bufferview_index.buffer].data.data() + bufferview_index.byteOffset);

			uint32_t processedIndices;
			switch (indexAccessor.componentType)
			{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: processedIndices = meshletBuilder.buildMeshlets<uint32_t>(meshlet, (uint32_t)indexAccessor.count, (uint32_t*)indices); break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: processedIndices = meshletBuilder.buildMeshlets(meshlet, (uint32_t)indexAccessor.count, (uint16_t*)indices); break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: processedIndices = meshletBuilder.buildMeshlets<uint8_t>(meshlet, (uint32_t)indexAccessor.count, (uint8_t*)indices); break;
			default: assert(false); break;
			}
			// std::cout << "warning: geometry meshlet incomplete" << std::endl;
			assert(processedIndices == indexAccessor.count);

			auto* vertex = (float*)(gltf_model.buffers[vertex_bufferview.buffer].data.data() + vertex_bufferview.byteOffset);
			meshletBuilder.buildMeshletEarlyCulling(meshlet, (float*)&primitive.m_aabb_min, (float*)&primitive.m_aabb_min, vertex, sizeof(float) * 3);

			{
#if defined(_DEBUG)
				NVMeshlet::StatusCode errorcode = NVMeshlet::STATUS_NO_ERROR;
				switch (indexAccessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: errorcode = meshletBuilder.errorCheck<uint32_t>(meshlet, 0, vertex_accessor.count, indexAccessor.count, (uint32_t*)indices); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: errorcode = meshletBuilder.errorCheck<uint16_t>(meshlet, 0, vertex_accessor.count, indexAccessor.count, (uint16_t*)indices); break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: errorcode = meshletBuilder.errorCheck<uint8_t>(meshlet, 0, vertex_accessor.count, indexAccessor.count, (uint8_t*)indices); break;
				default: assert(false); break;
				}
				if (errorcode)
				{
					std::cout << "geometry: meshlet error" << errorcode;
					assert(errorcode == NVMeshlet::STATUS_NO_ERROR);
				}

// 				NVMeshlet::Stats statsLocal;
// 				meshletBuilder.appendStats(meshlet, statsLocal);
// 				statsLocal.fprint(stdout);
#endif

			}

			meshlet.meshletDescriptors.resize(btr::align<uint32_t>(meshlet.meshletDescriptors.size(), 32));
			meshlet.meshletBboxes.resize(btr::align<uint32_t>(meshlet.meshletBboxes.size(), 32));

			static const uint32_t MESHLETS_PER_TASK = 32;
			primitive.m_task.setFirstTask(0);
			primitive.m_task.setTaskCount(meshlet.meshletDescriptors.size() / MESHLETS_PER_TASK);

			mesh.b_MeshletDesc[i] = ctx.m_vertex_memory.allocateMemory<NVMeshlet::MeshletPackBasicDesc>(meshlet.meshletDescriptors.size());
			mesh.b_MeshletPack[i] = ctx.m_vertex_memory.allocateMemory<NVMeshlet::PackBasicType>(meshlet.meshletPacks.size());

			{
				{
					auto staging = ctx.m_staging_memory.allocateMemory<NVMeshlet::MeshletPackBasicDesc>(meshlet.meshletDescriptors.size());
					memcpy_s(staging.getMappedPtr(), staging.getInfo().range, meshlet.meshletDescriptors.data(), vector_sizeof(meshlet.meshletDescriptors));
					vk::BufferCopy copy;
					copy.setSrcOffset(staging.getInfo().offset);
					copy.setDstOffset(mesh.b_MeshletDesc[i].getInfo().offset);
					copy.setSize(staging.getInfo().range);
					cmd.copyBuffer(staging.getInfo().buffer, mesh.b_MeshletDesc[i].getInfo().buffer, copy);

				}
				{
					auto staging = ctx.m_staging_memory.allocateMemory<NVMeshlet::PackBasicType>(meshlet.meshletPacks.size());
					memcpy_s(staging.getMappedPtr(), staging.getInfo().range, meshlet.meshletPacks.data(), vector_sizeof(meshlet.meshletPacks));
					vk::BufferCopy copy;
					copy.setSrcOffset(staging.getInfo().offset);
					copy.setDstOffset(mesh.b_MeshletPack[i].getInfo().offset);
					copy.setSize(staging.getInfo().range);
					cmd.copyBuffer(staging.getInfo().buffer, mesh.b_MeshletPack[i].getInfo().buffer, copy);
				}
			}
			primitive.MeshletDesc = mesh.b_MeshletDesc[i].getDeviceAddress();
			primitive.MeshletPack = mesh.b_MeshletPack[i].getDeviceAddress();
		}
		cmd.updateBuffer<Primitive>(mesh.b_primitive.getInfo().buffer, mesh.b_primitive.getInfo().offset, mesh.m_primitive);
	}

	// build BLAS
	{
		std::vector<vk::AccelerationStructureGeometryKHR> blas_geom;
		std::vector<uint32_t> primitive_count;
		std::vector<vk::AccelerationStructureBuildRangeInfoKHR> acceleration_buildrangeinfo;
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			auto& gltf_mesh = gltf_model.meshes[mesh_index];
			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
			{
				tinygltf::Primitive& primitive = gltf_mesh.primitives[i];
				auto vertex_attrib = primitive.attributes.find("POSITION");
				const tinygltf::Accessor& vertex_accessor = gltf_model.accessors[vertex_attrib->second];
				const tinygltf::BufferView& vertex_bufferview = gltf_model.bufferViews[vertex_accessor.bufferView];
				const tinygltf::Accessor& index_accessor = gltf_model.accessors[primitive.indices];
				const tinygltf::BufferView& index_bufferview = gltf_model.bufferViews[index_accessor.bufferView];

				vk::IndexType indextype = vk::IndexType::eUint32;
				switch (index_accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: indextype = vk::IndexType::eUint32; break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indextype = vk::IndexType::eUint16; break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: indextype = vk::IndexType::eUint8EXT; break;
				default: assert(false); break;
				}

				vk::AccelerationStructureGeometryKHR ASGeom;
				ASGeom.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
				ASGeom.geometryType = vk::GeometryTypeKHR::eTriangles;
				ASGeom.geometry.triangles.maxVertex = vertex_accessor.count;
				ASGeom.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
				ASGeom.geometry.triangles.vertexData.deviceAddress = resource->b_buffer[vertex_bufferview.buffer].getDeviceAddress() + vertex_bufferview.byteOffset;
				ASGeom.geometry.triangles.vertexStride = sizeof(vec3);
				ASGeom.geometry.triangles.indexType = indextype;
				ASGeom.geometry.triangles.indexData.deviceAddress = resource->b_buffer[index_bufferview.buffer].getDeviceAddress() + index_bufferview.byteOffset;
				blas_geom.push_back(ASGeom);

				primitive_count.push_back(index_accessor.count/3);

				acceleration_buildrangeinfo.emplace_back(index_accessor.count/3 , 0, 0, 0);
			}
		}

		vk::AccelerationStructureBuildGeometryInfoKHR AS_buildinfo;
		AS_buildinfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		AS_buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace /*| vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction*/;
		AS_buildinfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		AS_buildinfo.geometryCount = blas_geom.size();
		AS_buildinfo.pGeometries = blas_geom.data();

		auto size_info = ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, AS_buildinfo, primitive_count);

		auto AS_buffer = ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
		vk::AccelerationStructureCreateInfoKHR accelerationCI;
		accelerationCI.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		accelerationCI.buffer = AS_buffer.getInfo().buffer;
		accelerationCI.offset = AS_buffer.getInfo().offset;
		accelerationCI.size = AS_buffer.getInfo().range;
		auto AS = ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);
		AS_buildinfo.dstAccelerationStructure = AS.get();

		auto scratch_buffer = ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize, true);
		AS_buildinfo.scratchData.deviceAddress = scratch_buffer.getDeviceAddress();

		std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> ranges = { acceleration_buildrangeinfo.data() };
		cmd.buildAccelerationStructuresKHR({ AS_buildinfo }, ranges);

		// Compacting BLAS #todo queryがcpu側処理なので実装が難しい
		if (0)
		{
			vk::QueryPoolCreateInfo qp_ci;
			qp_ci.queryCount = 1;
			qp_ci.queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR;
			auto qp = ctx.m_device.createQueryPoolUnique(qp_ci);

			cmd.writeAccelerationStructuresPropertiesKHR(AS.get(), vk::QueryType::eAccelerationStructureCompactedSizeKHR, qp.get(), 0);


			vk::DeviceSize compactSize = ctx.m_device.getQueryPoolResult<vk::DeviceSize>(qp.get(), 0, 1, sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait).value;

			// Creating a compact version of the AS
			auto AS_compact_buffer = ctx.m_storage_memory.allocateMemory(compactSize);
			vk::AccelerationStructureCreateInfoKHR as_ci;
			as_ci.size = compactSize;
			as_ci.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
			accelerationCI.buffer = AS_compact_buffer.getInfo().buffer;
			accelerationCI.offset = AS_compact_buffer.getInfo().offset;
			accelerationCI.size = AS_compact_buffer.getInfo().range;
			auto AS_compact = ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);

			vk::CopyAccelerationStructureInfoKHR copy_info;
			copy_info.src = AS.get();
			copy_info.dst = AS_compact.get();
			copy_info.mode = vk::CopyAccelerationStructureModeKHR::eCompact;
			cmd.copyAccelerationStructureKHR(copy_info);

			resource->m_BLAS.m_AS = std::move(AS_compact);
			resource->m_BLAS.m_AS_buffer = std::move(AS_compact_buffer);

			vk::BufferMemoryBarrier barrier[] = { resource->m_BLAS.m_AS_buffer.makeMemoryBarrier(vk::AccessFlagBits::eAccelerationStructureWriteKHR, vk::AccessFlagBits::eAccelerationStructureReadKHR), };
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});

			sDeleter::Order().enque(std::move(AS), std::move(AS_buffer), std::move(scratch_buffer));
		}
		else
		{
			resource->m_BLAS.m_AS = std::move(AS);
			resource->m_BLAS.m_AS_buffer = std::move(AS_buffer);
			sDeleter::Order().enque(std::move(scratch_buffer));

		}
	}


	resource->gltf_model = std::move(gltf_model);
	resource->b_mesh = ctx.m_vertex_memory.allocateMemory<uint64_t>(resource->m_mesh.size());
	cmd.updateBuffer<uint64_t>(b_models.getInfo().buffer, b_models.getInfo().offset+resource->m_block*sizeof(uint64_t), { resource->b_mesh.getDeviceAddress()});

	std::vector<uint64_t> submesh_address(resource->m_mesh.size());
	for (size_t i = 0; i < submesh_address.size(); i++)
	{
		submesh_address[i] = resource->m_mesh[i].b_primitive.getDeviceAddress();
	}
	cmd.updateBuffer<uint64_t>(resource->b_mesh.getInfo().buffer, resource->b_mesh.getInfo().offset, submesh_address);

 	vk::BufferMemoryBarrier barrier[] = {
 		b_models.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
		resource->b_mesh.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
		resource->b_material.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead),
	};
 	cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, {}, { array_size(barrier), barrier }, {});

	return resource;
}


std::shared_ptr<gltf::Texture> Resource::LoadTexture(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename, const vk::ImageCreateInfo& info, const std::vector<byte>& data)
{
	std::shared_ptr<gltf::Texture> resource;
	if (m_texture_manager.getOrCreate(resource, filename))
	{
		return resource;
	}
	{
		vk::UniqueImage image = ctx.m_device.createImageUnique(info);

		vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(image.get());
		vk::MemoryAllocateInfo memory_alloc_info;
		memory_alloc_info.allocationSize = memory_request.size;
		memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::UniqueDeviceMemory image_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
		ctx.m_device.bindImageMemory(image.get(), image_memory.get(), 0);

		auto staging_buffer = ctx.m_staging_memory.allocateMemory<byte>(data.size(), true);
		memcpy(staging_buffer.getMappedPtr(), data.data(), data.size());

		vk::ImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.layerCount = 1;
		subresourceRange.levelCount = 1;

		{
			// staging_bufferからimageへコピー

			vk::BufferImageCopy copy;
			copy.bufferOffset = staging_buffer.getInfo().offset;
			copy.imageExtent = info.extent;
			copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
			copy.imageSubresource.baseArrayLayer = 0;
			copy.imageSubresource.layerCount = 1;
			copy.imageSubresource.mipLevel = 0;

			{
				vk::ImageMemoryBarrier to_copy_barrier;
				to_copy_barrier.image = image.get();
				to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
				to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_copy_barrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
			}
			cmd.copyBufferToImage(staging_buffer.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
			{
				vk::ImageMemoryBarrier to_shader_read_barrier;
				to_shader_read_barrier.image = image.get();
				to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				to_shader_read_barrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
			}

		}

		vk::ImageViewCreateInfo view_info;
		view_info.viewType = vk::ImageViewType::e2D;
		view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
		view_info.flags = vk::ImageViewCreateFlags();
		view_info.format = info.format;
		view_info.image = image.get();
		view_info.subresourceRange = subresourceRange;

		vk::SamplerCreateInfo sampler_info;
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.compareOp = vk::CompareOp::eNever;
		sampler_info.minLod = 0.0f;
		sampler_info.maxLod = 0.f;
		sampler_info.maxAnisotropy = 1.0;
		sampler_info.anisotropyEnable = VK_FALSE;
		sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

		resource->m_image = std::move(image);
		resource->m_memory = std::move(image_memory);
		resource->m_image_view = ctx.m_device.createImageViewUnique(view_info);
		resource->m_sampler = ctx.m_device.createSamplerUnique(sampler_info);
	}

	m_image_info[resource->m_block] = resource->info();
#if defined(USE_TEMPLATE) // updateDescriptorSetWithTemplateがバグってる
	ctx.m_device.updateDescriptorSetWithTemplate(*m_DS_ModelResource, *m_Texture_DUP, this);
#else
	vk::WriteDescriptorSet write[] =
	{
		vk::WriteDescriptorSet().setDstSet(*m_DS_ModelResource).setDstBinding(0).setDescriptorType(vk::DescriptorType::eStorageBuffer).setBufferInfo(m_buffer_info),
		vk::WriteDescriptorSet().setDstSet(*m_DS_ModelResource).setDstBinding(100).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setImageInfo(m_image_info),
	};
	ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
#endif
	return resource;
}
