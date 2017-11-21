#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <btrlib/DefineMath.h>
#include <btrlib/sGlobal.h>
struct cMotion
{
	struct VecKey
	{
		float m_time;
		glm::vec3 m_data;
	};
	struct QuatKey
	{
		float m_time;
		glm::quat m_data;
	};
	struct NodeMotion
	{
		uint32_t m_node_index;
		std::string m_nodename;
		std::vector<VecKey> m_translate;
		std::vector<VecKey> m_scale;
		std::vector<QuatKey> m_rotate;
	};
	std::string m_name;
	float m_duration;
	float m_ticks_per_second;
	uint32_t m_node_num;
	std::vector<NodeMotion> m_data;
};
struct cAnimation
{
	std::vector<std::shared_ptr<cMotion>> m_motion;
};


/**
* animationデータを格納したテクスチャ
*/
struct MotionTexture
{
	struct Resource
	{
		vk::UniqueImage m_image;
		vk::UniqueImageView m_image_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueSampler m_sampler;
		~Resource()
		{
			if (m_image)
			{
				sDeleter::Order().enque(std::move(m_image), std::move(m_image_view), std::move(m_memory), std::move(m_sampler));
			}
		}
	};

	std::shared_ptr<Resource> m_resource;

	vk::ImageView getImageView()const { return m_resource ? m_resource->m_image_view.get() : vk::ImageView(); }
	vk::Sampler getSampler()const { return m_resource ? m_resource->m_sampler.get() : vk::Sampler(); }

	static std::vector<MotionTexture> createMotion(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd, const cAnimation& anim)
	{
		std::vector<MotionTexture> motion_texture(anim.m_motion.size());
		for (size_t i = 0; i < anim.m_motion.size(); i++)
		{
			motion_texture[i] = MotionTexture::create(context, cmd, anim.m_motion[i]);
		}
		return motion_texture;
	}

	/**
	*	モーションのデータを一枚の1DArrayに格納
	*/
	static MotionTexture create(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd, const std::shared_ptr<cMotion>& motion)
	{
		uint32_t SIZE = 256;

		vk::ImageCreateInfo image_info;
		image_info.imageType = vk::ImageType::e1D;
		image_info.format = vk::Format::eR16G16B16A16Sfloat;
		image_info.mipLevels = 1;
		image_info.arrayLayers = motion->m_node_num * 3u;
		image_info.samples = vk::SampleCountFlagBits::e1;
		image_info.tiling = vk::ImageTiling::eOptimal;
		image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
		image_info.sharingMode = vk::SharingMode::eExclusive;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
//		image_info.flags = vk::ImageCreateFlagBits::eMutableFormat;
		image_info.extent = { SIZE, 1u, 1u };

		auto image_prop = context->m_device.getGPU().getImageFormatProperties(image_info.format, image_info.imageType, image_info.tiling, image_info.usage, image_info.flags);
		vk::UniqueImage image = context->m_device->createImageUnique(image_info);

		btr::BufferMemoryDescriptor staging_desc;
		staging_desc.size = image_info.arrayLayers * motion->m_data.size() * SIZE * sizeof(glm::u16vec4);
		staging_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
		auto staging_buffer = context->m_staging_memory.allocateMemory(staging_desc);
		auto* data = staging_buffer.getMappedPtr<glm::u16vec4>();

		for (size_t i = 0; i < motion->m_data.size(); i++)
		{
			const cMotion::NodeMotion& node_motion = motion->m_data[i];
			auto no = node_motion.m_node_index;
			auto* pos = reinterpret_cast<uint64_t*>(data + no * 3 * SIZE);
			auto* quat = reinterpret_cast<uint64_t*>(pos + SIZE);
			auto* scale = reinterpret_cast<uint64_t*>(quat + SIZE);

			float time_max = motion->m_duration;// / anim->mTicksPerSecond;
			float time_per = time_max / SIZE;
			float time = 0.;
			int count = 0;
			for (size_t n = 0; n < node_motion.m_scale.size() - 1;)
			{
				auto& current = node_motion.m_scale[n];
				auto& next = node_motion.m_scale[n + 1];

				if (time >= next.m_time)
				{
					n++;
					continue;
				}
				auto current_value = glm::vec4(current.m_data.x, current.m_data.y, current.m_data.z, 1.f);
				auto next_value = glm::vec4(next.m_data.x, next.m_data.y, next.m_data.z, 1.f);
				auto rate = 1.f - (next.m_time - time) / (next.m_time - current.m_time);
				auto value = glm::lerp(current_value, next_value, rate);
				scale[count] = glm::packHalf4x16(value);
				count++;
				time += time_per;
			}
			assert(count == SIZE);
			time = 0.;
			count = 0;
			for (size_t n = 0; n < node_motion.m_translate.size() - 1;)
			{
				auto& current = node_motion.m_translate[n];
				auto& next = node_motion.m_translate[n + 1];

				if (time >= next.m_time)
				{
					n++;
					continue;
				}
				auto current_value = glm::vec3(current.m_data.x, current.m_data.y, current.m_data.z);
				auto next_value = glm::vec3(next.m_data.x, next.m_data.y, next.m_data.z);
				auto rate = 1.f - (float)(next.m_time - time) / (float)(next.m_time - current.m_time);
				auto value = glm::lerp<float>(current_value, next_value, rate);
				pos[count] = glm::packHalf4x16(glm::vec4(value, 0.f));
				count++;
				time += time_per;
			}
			assert(count == SIZE);
			time = 0.;
			count = 0;
			for (size_t n = 0; n < node_motion.m_rotate.size() - 1;)
			{
				auto& current = node_motion.m_rotate[n];
				auto& next = node_motion.m_rotate[n + 1];

				if (time >= next.m_time)
				{
					n++;
					continue;
				}
				auto current_value = current.m_data;
				auto next_value = next.m_data;
				auto rate = 1.f - (float)(next.m_time - time) / (float)(next.m_time - current.m_time);
				auto value = glm::slerp<float>(current_value, next_value, rate);
				quat[count] = glm::packHalf4x16(glm::vec4(value.x, value.y, value.z, value.w));
				count++;
				time += time_per;
			}
			assert(count == SIZE);
		}

		vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(image.get());
		vk::MemoryAllocateInfo memory_alloc_info;
		memory_alloc_info.allocationSize = memory_request.size;
		memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::UniqueDeviceMemory image_memory = context->m_device->allocateMemoryUnique(memory_alloc_info);
		context->m_device->bindImageMemory(image.get(), image_memory.get(), 0);

		vk::ImageSubresourceRange subresourceRange;
		subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		subresourceRange.baseArrayLayer = 0;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.layerCount = image_info.arrayLayers;
		subresourceRange.levelCount = 1;

		vk::BufferImageCopy copy;
		copy.bufferOffset = staging_buffer.getBufferInfo().offset;
		copy.imageExtent = { SIZE, 1u, 1u };
		copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = image_info.arrayLayers;
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
		cmd.copyBufferToImage(staging_buffer.getBufferInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, { copy });
		{
			vk::ImageMemoryBarrier to_shader_read_barrier;
			to_shader_read_barrier.image = image.get();
			to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			to_shader_read_barrier.subresourceRange = subresourceRange;

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
		}

		vk::ImageViewCreateInfo view_info;
		view_info.viewType = vk::ImageViewType::e1DArray;
		view_info.components.r = vk::ComponentSwizzle::eR;
		view_info.components.g = vk::ComponentSwizzle::eG;
		view_info.components.b = vk::ComponentSwizzle::eB;
		view_info.components.a = vk::ComponentSwizzle::eA;
		view_info.flags = vk::ImageViewCreateFlags();
		view_info.format = image_info.format;
		view_info.image = image.get();
		view_info.subresourceRange = subresourceRange;

		vk::SamplerCreateInfo sampler_info;
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;//
		sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.compareOp = vk::CompareOp::eNever;
		sampler_info.minLod = 0.0f;
		sampler_info.maxLod = 0.f;
		sampler_info.maxAnisotropy = 1.0;
		sampler_info.anisotropyEnable = VK_FALSE;
		sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

		MotionTexture tex;
		tex.m_resource = std::make_shared<MotionTexture::Resource>();
		tex.m_resource->m_image = std::move(image);
		tex.m_resource->m_image_view = context->m_device->createImageViewUnique(view_info);
		tex.m_resource->m_memory = std::move(image_memory);
		tex.m_resource->m_sampler = context->m_device->createSamplerUnique(sampler_info);
		return tex;
	}
};

struct PlayMotionDescriptor
{
	std::shared_ptr<cMotion> m_data;
	uint32_t m_play_no;
	uint32_t m_is_loop;
	float m_start_time;

	PlayMotionDescriptor()
		: m_play_no(0)
		, m_is_loop(false)
		, m_start_time(0.f)
	{

	}

};
struct MotionPlayList
{
	struct Work
	{
		std::shared_ptr<cMotion> m_motion;
		float m_time;		//!< 再生位置
		int m_index;
		bool m_is_playing;	//!< 再生中？

		Work()
			: m_time(0.f)
			, m_is_playing(false)

		{}
	};
	std::array<Work, 8> m_work;

	void execute()
	{
		float dt = sGlobal::Order().getDeltaTime();
		for (auto& work : m_work)
		{
			if (!work.m_is_playing)
			{
				continue;
			}
			work.m_time += dt * work.m_motion->m_ticks_per_second;
		}
	}

	void play(const PlayMotionDescriptor& desc)
	{
		m_work[desc.m_play_no].m_motion = desc.m_data;
		m_work[desc.m_play_no].m_time = desc.m_start_time;
		m_work[desc.m_play_no].m_index = 0;
		m_work[desc.m_play_no].m_is_playing = true;
	}
};

struct ModelTransform
{
	glm::vec3 m_local_scale;
	glm::quat m_local_rotate;
	glm::vec3 m_local_translate;

	glm::mat4 m_global;

	ModelTransform()
		: m_local_scale(1.f)
		, m_local_rotate(1.f, 0.f, 0.f, 0.f)
		, m_local_translate(0.f)
		, m_global(1.f)
	{}
	glm::mat4 calcLocal()const
	{
		return glm::scale(m_local_scale) * glm::toMat4(m_local_rotate) * glm::translate(m_local_translate);
	}
	glm::mat4 calcGlobal()const
	{
		return m_global;
	}
};

struct AnimationModule
{
	ModelTransform m_model_transform;
	MotionPlayList m_playlist;

	ModelTransform& getTransform() { return m_model_transform; }
	const ModelTransform& getTransform()const { return m_model_transform; }
	MotionPlayList& getPlayList() { return m_playlist; }
	const MotionPlayList& getPlayList()const { return m_playlist; }

	virtual vk::DescriptorBufferInfo getBoneBuffer()const = 0;
	virtual void animationUpdate() = 0;
	virtual void animationExecute(const std::shared_ptr<btr::Context>& context, vk::CommandBuffer& cmd) = 0;
};

struct InstancingAnimationModule : public AnimationModule
{
	virtual vk::DescriptorBufferInfo getModelInfo()const = 0;
	virtual vk::DescriptorBufferInfo getInstancingInfo()const = 0;
	virtual vk::DescriptorBufferInfo getAnimationInfoBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getPlayingAnimationBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getNodeBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getBoneMap()const = 0;
	virtual vk::DescriptorBufferInfo getNodeInfoBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getBoneInfoBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getWorldBuffer()const = 0;
	virtual vk::DescriptorBufferInfo getDrawIndirect()const = 0;
	virtual const std::vector<MotionTexture>& getMotionTexture()const = 0;

};

