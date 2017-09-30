#pragma once
#include <vector>
#include <btrlib/Define.h>
#include <btrlib/GPU.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/Context.h>
#include <btrlib/ResourceManager.h>



struct ResourceTexture
{
	struct Resource
	{
		std::string m_filename;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;
	};

	std::shared_ptr<Resource> m_resource;

	void load(std::shared_ptr<btr::Context>& loader, vk::CommandBuffer cmd, const std::string& filename);
	vk::ImageView getImageView()const { return m_resource ? m_resource->m_image_view.get() : vk::ImageView(); }
	vk::Sampler getSampler()const { return m_resource ? m_resource->m_sampler.get() : vk::Sampler(); }

	bool isReady()const
	{
		return !!m_resource;
	}

	static ResourceManager<Resource> s_manager;

};

struct MaterialModule
{
	virtual const btr::BufferMemory& getMaterialIndexBuffer()const = 0;
	virtual const btr::BufferMemory& getMaterialBuffer()const = 0;
	virtual const std::vector<ResourceTexture>& getTextureList()const = 0;
};