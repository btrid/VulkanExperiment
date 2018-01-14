#pragma once
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <array>
#include <btrlib/Define.h>
#include <btrlib/Context.h>
#include <btrlib/ResourceManager.h>


class rTexture
{
public:
	struct LoadParam
	{
		bool isFlipY;
		LoadParam()
			: isFlipY(false)
		{

		}
	};

	struct Data{
		std::vector<glm::vec4> m_data;
		glm::uvec3 m_size;

		size_t getBufferSize()const { return m_data.size() * sizeof(decltype(m_data)::value_type); }
	};
	static Data LoadTexture(const std::string& file, const LoadParam& param = LoadParam());

};

struct ResourceTexture
{
	struct Resource
	{
		std::string m_filename;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;

		~Resource()
		{
			int a = 0;
		}
	};

	std::shared_ptr<Resource> m_resource;

	void load(const std::shared_ptr<btr::Context>& loader, vk::CommandBuffer cmd, const std::string& filename);
	vk::ImageView getImageView()const { return m_resource ? m_resource->m_image_view.get() : vk::ImageView(); }
	vk::Sampler getSampler()const { return m_resource ? m_resource->m_sampler.get() : vk::Sampler(); }

	bool isReady()const
	{
		return !!m_resource;
	}

	template<class Archive>
	void serialize(Archive & archive)
	{
	}

	static ResourceManager<Resource> s_manager;

};

