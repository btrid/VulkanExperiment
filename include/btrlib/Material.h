#pragma once
#include <memory>
#include <vector>
#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/ResourceManager.h>
#include <btrlib/cModel.h>
#include <btrlib/rTexture.h>

struct MaterialModule
{
	virtual vk::DescriptorBufferInfo getMaterialIndexBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getMaterialBuffer()const = 0;
	virtual const std::vector<ResourceTexture>& getTextureList()const = 0;
};

struct DefaultMaterialModule : public MaterialModule
{
	struct MaterialBuffer {
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		glm::vec4		mEmissive;
		uint32_t		u_albedo_texture;
		uint32_t		u_ambient_texture;
		uint32_t		u_specular_texture;
		uint32_t		u_emissive_texture;
		float			mShininess;
		float			__p;
		float			__p1;
		float			__p2;
	};

	btr::BufferMemory m_material_index;
	btr::BufferMemory m_material;
	std::vector<ResourceTexture> m_texture;

	DefaultMaterialModule(const std::shared_ptr<btr::Context>& context, const std::shared_ptr<cModel::Resource>& resource)
	{
		auto cmd = context->m_cmd_pool->allocCmdTempolary(0);

		// material index
		{
			btr::BufferMemoryDescriptor staging_desc;
			staging_desc.size = resource->m_mesh.size() * sizeof(uint32_t);
			staging_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging = context->m_staging_memory.allocateMemory(staging_desc);

			std::vector<uint32_t> material_index(resource->m_mesh.size());
			for (size_t i = 0; i < material_index.size(); i++)
			{
				staging.getMappedPtr<uint32_t>()[i] = resource->m_mesh[i].m_material_index;
			}

			m_material_index = context->m_storage_memory.allocateMemory(material_index.size() * sizeof(uint32_t));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging.getBufferInfo().range);
			copy_info.setSrcOffset(staging.getBufferInfo().offset);
			copy_info.setDstOffset(m_material_index.getBufferInfo().offset);
			cmd->copyBuffer(staging.getBufferInfo().buffer, m_material_index.getBufferInfo().buffer, copy_info);

		}

		// material
		{
			btr::BufferMemoryDescriptor staging_desc;
			staging_desc.size = resource->m_material.size() * sizeof(MaterialBuffer);
			staging_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
			auto staging_material = context->m_staging_memory.allocateMemory(staging_desc);
			auto* mb = static_cast<MaterialBuffer*>(staging_material.getMappedPtr());
			for (size_t i = 0; i < resource->m_material.size(); i++)
			{
				mb[i].mAmbient = resource->m_material[i].mAmbient;
				mb[i].mDiffuse = resource->m_material[i].mDiffuse;
				mb[i].mEmissive = resource->m_material[i].mEmissive;
				mb[i].mSpecular = resource->m_material[i].mSpecular;
				mb[i].mShininess = resource->m_material[i].mShininess;
			}

			m_material = context->m_storage_memory.allocateMemory(resource->m_material.size() * sizeof(MaterialBuffer));

			vk::BufferCopy copy_info;
			copy_info.setSize(staging_material.getBufferInfo().range);
			copy_info.setSrcOffset(staging_material.getBufferInfo().offset);
			copy_info.setDstOffset(m_material.getBufferInfo().offset);
			cmd->copyBuffer(staging_material.getBufferInfo().buffer, m_material.getBufferInfo().buffer, copy_info);
		}

		// todo Œ‹\“K“–
		m_texture.resize(resource->m_material.size() * 1);
		for (size_t i = 0; i < resource->m_material.size(); i++)
		{
			auto& m = resource->m_material[i];
			m_texture[i * 1 + 0] = m.mDiffuseTex.isReady() ? m.mDiffuseTex : ResourceTexture();
		}
	}

	virtual vk::DescriptorBufferInfo getMaterialIndexBuffer()const override { return m_material_index.getBufferInfo(); }
	virtual vk::DescriptorBufferInfo getMaterialBuffer()const override { return m_material.getBufferInfo(); }
	virtual const std::vector<ResourceTexture>& getTextureList()const { return m_texture; }

};
