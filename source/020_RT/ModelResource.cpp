#include <020_RT/ModelResource.h>

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

std::shared_ptr<Model> ModelResource::LoadModel(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename)
{
	std::shared_ptr<Model> model;
	if (m_model_manager.getOrCreate(model, filename)) {
		return model;
	}

	tinygltf::Model gltf_model;
	{
		tinygltf::TinyGLTF loader;
		//			loader.SetImageLoader()
		std::string err;
		std::string warn;

		//			loader.
		bool res = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, filename);
		if (!warn.empty()) { std::cout << "WARN: " << warn << std::endl; }
		if (!err.empty()) { std::cout << "ERR: " << err << std::endl; }

		if (!res) std::cout << "Failed to load glTF: " << filename << std::endl;
		else std::cout << "Loaded glTF: " << filename << std::endl;
	}

	// buffer
	{
		model->b_vertex.resize(gltf_model.buffers.size());
		std::vector<btr::BufferMemory> staging(gltf_model.buffers.size());
		for (int i = 0; i < gltf_model.buffers.size(); i++)
		{
			model->b_vertex[i] = ctx.m_vertex_memory.allocateMemory(gltf_model.buffers[i].data.size());
			staging[i] = ctx.m_staging_memory.allocateMemory(gltf_model.buffers[i].data.size());
			memcpy_s(staging[i].getMappedPtr(), gltf_model.buffers[i].data.size(), gltf_model.buffers[i].data.data(), gltf_model.buffers[i].data.size());

			vk::BufferCopy copy = vk::BufferCopy().setSize(gltf_model.buffers[i].data.size()).setSrcOffset(staging[i].getInfo().offset).setDstOffset(model->b_vertex[i].getInfo().offset);
			cmd.copyBuffer(staging[i].getInfo().buffer, model->b_vertex[i].getInfo().buffer, copy);
		}
	}

	// image 
	{
		model->t_image.resize(gltf_model.images.size());
		for (size_t i = 0; i < gltf_model.images.size(); i++)
		{
			tinygltf::Texture& tex = gltf_model.textures[i];
			tinygltf::Image& image = gltf_model.images[i];

			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR8G8B8A8Unorm;//GLtoVK::toFormat(image.pixel_type, image.component, image.bits);
			image_info.mipLevels = 1;
			image_info.arrayLayers = 1;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eLinear;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.extent = vk::Extent3D(image.width, image.height, 1);

			model->t_image[i] = LoadTexture(ctx, cmd, image.uri, image_info, image.image);
		}
	}
	// material
	{
		std::vector<Model::Material> material(gltf_model.materials.size());
		for (size_t i = 0; i < gltf_model.materials.size(); i++)
		{
			auto gltf_material = gltf_model.materials[i];
			Model::Material& m = material[i];

			m.m_basecolor_factor.x = gltf_material.pbrMetallicRoughness.baseColorFactor[0];
			m.m_basecolor_factor.y = gltf_material.pbrMetallicRoughness.baseColorFactor[1];
			m.m_basecolor_factor.z = gltf_material.pbrMetallicRoughness.baseColorFactor[2];
			m.m_basecolor_factor.w = gltf_material.pbrMetallicRoughness.baseColorFactor[3];
			m.m_metallic_factor = gltf_material.pbrMetallicRoughness.metallicFactor;
			m.m_roughness_factor = gltf_material.pbrMetallicRoughness.roughnessFactor;
			m.m_emissive_factor.x = gltf_material.emissiveFactor[0];
			m.m_emissive_factor.y = gltf_material.emissiveFactor[1];
			m.m_emissive_factor.z = gltf_material.emissiveFactor[2];

			m.t[Model::Material::TexID_Base] = -1;
			if (gltf_material.pbrMetallicRoughness.baseColorTexture.index >= 0)
			{
				m.t[Model::Material::TexID_Base] = model->t_image[gltf_material.pbrMetallicRoughness.baseColorTexture.index]->m_block;
			}
			m.t[Model::Material::TexID_MR] = -1;
			if (gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
			{
				m.t[Model::Material::TexID_MR] = model->t_image[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index]->m_block;
			}
			m.t[Model::Material::TexID_Normal] = -1;
			if (gltf_material.normalTexture.index >= 0)
			{
				m.t[Model::Material::TexID_Normal] = model->t_image[gltf_material.normalTexture.index]->m_block;
			}
			m.t[Model::Material::TexID_AO] = -1;
			if (gltf_material.occlusionTexture.index >= 0)
			{
				m.t[Model::Material::TexID_AO] = model->t_image[gltf_material.occlusionTexture.index]->m_block;
			}
			m.t[Model::Material::TexID_Emissive] = -1;
			if (gltf_material.emissiveTexture.index >= 0)
			{
				m.t[Model::Material::TexID_Emissive] = model->t_image[gltf_material.emissiveTexture.index]->m_block;
			}
		}
		model->b_material = ctx.m_vertex_memory.allocateMemory<Model::Material>(material.size());
		cmd.updateBuffer<Model::Material>(model->b_material.getInfo().buffer, model->b_material.getInfo().offset, material);

		model->m_entity.v[Entity::Material] = model->b_material.getDeviceAddress();
	}


	for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
	{
		auto& gltf_mesh = gltf_model.meshes[mesh_index];
		for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
		{
			tinygltf::Primitive& primitive = gltf_mesh.primitives[i];
			tinygltf::Accessor& indexAccessor = gltf_model.accessors[primitive.indices];
			tinygltf::BufferView& bufferview_index = gltf_model.bufferViews[indexAccessor.bufferView];
			model->m_entity.v[Entity::Index] = model->b_vertex[bufferview_index.buffer].getDeviceAddress() + bufferview_index.byteOffset;

			for (auto& attrib : primitive.attributes)
			{
				tinygltf::Accessor& accessor = gltf_model.accessors[attrib.second];
				tinygltf::BufferView& bufferview = gltf_model.bufferViews[accessor.bufferView];

				int size = 1;
				if (accessor.type != TINYGLTF_TYPE_SCALAR)
				{
					size = accessor.type;
				}

				if (attrib.first.compare("POSITION") == 0)
				{
					model->m_entity.v[Entity::Vertex] = model->b_vertex[bufferview.buffer].getDeviceAddress() + bufferview.byteOffset;
				}
				else if (attrib.first.compare("NORMAL") == 0)
				{
					model->m_entity.v[Entity::Normal] = model->b_vertex[bufferview.buffer].getDeviceAddress() + bufferview.byteOffset;
				}
				else if (attrib.first.compare("TEXCOORD_0") == 0)
				{
					model->m_entity.v[Entity::Texcoord] = model->b_vertex[bufferview.buffer].getDeviceAddress() + bufferview.byteOffset;
				}
				else
				{
					assert(false);
				}
			}

			model->m_entity.primitive_num = indexAccessor.count / 3;
		}
	}

	cmd.updateBuffer<Entity>(b_model_entity.getInfo().buffer, b_model_entity.getInfo().offset + sizeof(Entity) * model->m_block, model->m_entity);


	// build BLAS
	auto& _ctx = ctx;
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
				const tinygltf::Accessor& accessor = gltf_model.accessors[primitive.indices];
				const tinygltf::BufferView& bufferview = gltf_model.bufferViews[accessor.bufferView];

				vk::IndexType indextype = vk::IndexType::eUint32;
				switch (accessor.componentType)
				{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: indextype = vk::IndexType::eUint32; break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indextype = vk::IndexType::eUint16; break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: indextype = vk::IndexType::eUint8EXT; break;
				default: assert(false); break;
				}

				vk::AccelerationStructureGeometryKHR ASGeom;
				ASGeom.flags = vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
				ASGeom.geometryType = vk::GeometryTypeKHR::eTriangles;
				ASGeom.geometry.triangles.maxVertex = model->m_info.m_vertex_num;
				ASGeom.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
				ASGeom.geometry.triangles.vertexData.deviceAddress = model->b_vertex[vertex_bufferview.buffer].getDeviceAddress();
				ASGeom.geometry.triangles.vertexStride = sizeof(vec3);
				ASGeom.geometry.triangles.indexType = indextype;
				ASGeom.geometry.triangles.indexData.deviceAddress = model->b_vertex[bufferview.buffer].getDeviceAddress();
				blas_geom.push_back(ASGeom);

				primitive_count.push_back(accessor.count);

				acceleration_buildrangeinfo.emplace_back(accessor.count, 0, 0, 0);
			}
		}

		vk::AccelerationStructureBuildGeometryInfoKHR AS_buildinfo;
		AS_buildinfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		AS_buildinfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace /*| vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction*/;
		AS_buildinfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
		AS_buildinfo.geometryCount = blas_geom.size();
		AS_buildinfo.pGeometries = blas_geom.data();

		auto size_info = _ctx.m_device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, AS_buildinfo, primitive_count);

		auto AS_buffer = _ctx.m_storage_memory.allocateMemory(size_info.accelerationStructureSize);
		vk::AccelerationStructureCreateInfoKHR accelerationCI;
		accelerationCI.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
		accelerationCI.buffer = AS_buffer.getInfo().buffer;
		accelerationCI.offset = AS_buffer.getInfo().offset;
		accelerationCI.size = AS_buffer.getInfo().range;
		auto AS = _ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);
		AS_buildinfo.dstAccelerationStructure = AS.get();

		auto scratch_buffer = _ctx.m_storage_memory.allocateMemory(size_info.buildScratchSize, true);
		AS_buildinfo.scratchData.deviceAddress = scratch_buffer.getDeviceAddress();

		std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> ranges = { acceleration_buildrangeinfo.data() };
		cmd.buildAccelerationStructuresKHR({ AS_buildinfo }, ranges);

		// Compacting BLAS #todo queryがcpu側処理なので実装が難しい
		if (0)
		{
			vk::QueryPoolCreateInfo qp_ci;
			qp_ci.queryCount = 1;
			qp_ci.queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR;
			auto qp = _ctx.m_device.createQueryPoolUnique(qp_ci);

			cmd.writeAccelerationStructuresPropertiesKHR(AS.get(), vk::QueryType::eAccelerationStructureCompactedSizeKHR, qp.get(), 0);


			vk::DeviceSize compactSize = _ctx.m_device.getQueryPoolResult<vk::DeviceSize>(qp.get(), 0, 1, sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait).value;

			// Creating a compact version of the AS
			auto AS_compact_buffer = _ctx.m_storage_memory.allocateMemory(compactSize);
			vk::AccelerationStructureCreateInfoKHR as_ci;
			as_ci.size = compactSize;
			as_ci.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
			accelerationCI.buffer = AS_compact_buffer.getInfo().buffer;
			accelerationCI.offset = AS_compact_buffer.getInfo().offset;
			accelerationCI.size = AS_compact_buffer.getInfo().range;
			auto AS_compact = _ctx.m_device.createAccelerationStructureKHRUnique(accelerationCI);

			vk::CopyAccelerationStructureInfoKHR copy_info;
			copy_info.src = AS.get();
			copy_info.dst = AS_compact.get();
			copy_info.mode = vk::CopyAccelerationStructureModeKHR::eCompact;
			cmd.copyAccelerationStructureKHR(copy_info);

			model->m_BLAS.m_AS = std::move(AS_compact);
			model->m_BLAS.m_AS_buffer = std::move(AS_compact_buffer);

			vk::BufferMemoryBarrier barrier[] = { model->m_BLAS.m_AS_buffer.makeMemoryBarrier(vk::AccessFlagBits::eAccelerationStructureWriteKHR, vk::AccessFlagBits::eAccelerationStructureReadKHR), };
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, {}, {}, { array_size(barrier), barrier }, {});

			sDeleter::Order().enque(std::move(AS), std::move(AS_buffer), std::move(scratch_buffer));
		}
		else
		{
			model->m_BLAS.m_AS = std::move(AS);
			model->m_BLAS.m_AS_buffer = std::move(AS_buffer);
			sDeleter::Order().enque(std::move(scratch_buffer));

		}
	}

// 	{
// 		vk::DescriptorSetLayout layouts[] =
// 		{
// 			ctx.m_DSL[Context::DSL_Model_Entity].get(),
// 		};
// 		vk::DescriptorSetAllocateInfo desc_info;
// 		desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
// 		desc_info.setDescriptorSetCount(array_length(layouts));
// 		desc_info.setPSetLayouts(layouts);
// 		model->m_DS_Model = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);
// 		{
// 			vk::DescriptorBufferInfo storages[] =
// 			{
// 				model->b_entity.getInfo(),
// 			};
// 			vk::WriteDescriptorSet write[] =
// 			{
// 				vk::WriteDescriptorSet().setDstSet(*model->m_DS_Model).setDstBinding(0).setDescriptorType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(array_length(storages)).setPBufferInfo(storages),
// 			};
// 			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
// 		}
// 	}
	model->gltf_model = std::move(gltf_model);
	return model;
}

std::shared_ptr<ModelTexture> ModelResource::LoadTexture(btr::Context& ctx, vk::CommandBuffer cmd, const std::string& filename, const vk::ImageCreateInfo& info, const std::vector<byte>& data)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_resource_list.find(filename);
	if (it != m_resource_list.end())
	{
		return it->second.lock();
	}
	auto deleter = [&](ModelTexture* ptr) { release(ptr); sDeleter::Order().enque(std::unique_ptr<ModelTexture>(ptr)); };
	auto resource = std::shared_ptr<ModelTexture>(new ModelTexture, deleter);
	resource->m_filename = filename;
	resource->m_block = m_active[m_consume];
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
	ctx.m_device.updateDescriptorSetWithTemplate(*m_DS_ModelResource, *m_Texture_DUP, this);

	m_resource_list[filename] = resource;
	m_consume = (m_consume + 1) % m_active.size();
	return resource;
}
