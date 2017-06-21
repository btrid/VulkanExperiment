#pragma once
#include <memory>
#include <btrlib/cModel.h>
#include <btrlib/Light.h>
#include <btrlib/BufferMemory.h>
#include <applib/Loader.h>
#include <002_model/cModelPipeline.h>
#include <002_model/cLightPipeline.h>
struct cModelRenderer;

/**
* animationデータを格納したテクスチャ
*/
struct MotionTexture
{
	struct Resource
	{
		cDevice m_device;
		vk::FenceShared m_fence_shared;
		vk::Image m_image;
		vk::ImageView m_image_view;
		vk::DeviceMemory m_memory;
		vk::Sampler m_sampler;
		~Resource()
		{
			if (m_image)
			{
				std::unique_ptr<Deleter> deleter = std::make_unique<Deleter>();
				deleter->device = m_device.getHandle();
				deleter->image = { m_image };
				//				deleter->sampler = { m_sampler };
				deleter->memory = { m_memory };
				deleter->fence_shared = { m_fence_shared };
				sGlobal::Order().destroyResource(std::move(deleter));

				m_device->destroyImageView(m_image_view);
			}
		}
	};

	std::shared_ptr<Resource> m_resource;

	vk::ImageView getImageView()const { return m_resource ? m_resource->m_image_view : vk::ImageView(); }

	bool isReady()const
	{
		return m_resource ? m_resource->m_device->getFenceStatus(*m_resource->m_fence_shared) == vk::Result::eSuccess : true;
	}
};
class ModelRender
{
public:
	struct MaterialBuffer {
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		glm::vec4		mEmissive;
		std::uint64_t	mDiffuseTex;
		std::uint64_t	mAmbientTex;
		std::uint64_t	mSpecularTex;
		std::uint64_t	__pp;
		float			mShininess;
		float			__p;
		float			__p1;
		float			__p2;
	};
	struct AnimationInfo
	{
		//	int animationNo_;
		float duration_;
		float ticksPerSecond_;
		int numInfo_;
		int offsetInfo_;
	};
	struct PlayingAnimation
	{
		int		playingAnimationNo;	//!< モーション
		float	time;				//!< モーションの再生時間 
		int		currentMotionInfoIndex;	//!< 今再生しているモーションデータのインデックス
		int		isLoop;				//!<
	};

	struct BoneInfo
	{
		s32 mNodeIndex;
		s32 _p[3];
		glm::mat4 mBoneOffset;
	};
	struct ModelInstancingInfo {
		s32 mInstanceMaxNum;		//!< 出せるモデルの量
		s32 mInstanceAliveNum;		//!< 生きているモデルの量
		s32 mInstanceNum;			//!< 実際に描画する数
		s32 _p[1];
	};
public:
	struct NodeInfo {
		int32_t		mNodeNo;
		int32_t		mParent;
		int32_t		mBoneIndex;
		int32_t		m_depth;	//!< RootNodeからの深さ

		NodeInfo()
			: mNodeNo(-1)
			, mParent(-1)
			, mBoneIndex(-1)
			, m_depth(0)
		{
		}
	};

	struct NodeTransformBuffer {
		glm::mat4	localAnimated_;		//!< parentMatrix,Worldをかけていない行列
		glm::mat4	globalAnimated_;
	};
	struct NodeLocalTransformBuffer {
		glm::mat4	localAnimated_;		//!< parentMatrix,Worldをかけていない行列
	};
	struct NodeGlobalTransformBuffer {
		glm::mat4	globalAnimated_;
	};

	struct BoneTransformBuffer {
		glm::mat4	value_;
	};

	std::shared_ptr<cModel::Resource> m_resource;
	std::vector<cModel*> m_model;

	std::vector<vk::DescriptorSet> m_descriptor_set;
	vk::DescriptorSet m_draw_descriptor_set_per_model;
	std::vector<vk::DescriptorSet> m_draw_descriptor_set_per_mesh;
	std::vector<vk::DescriptorSet> m_compute_descriptor_set;

	enum ModelStorageBuffer : s32
	{
		MODEL_INFO,
		MODEL_INSTANCING_INFO,
		NODE_INFO,
		BONE_INFO,
		PLAYING_ANIMATION,
		MATERIAL_INDEX,
		MATERIAL,
		VS_MATERIAL,	// vertex stage material
		NODE_LOCAL_TRANSFORM,
		NODE_GLOBAL_TRANSFORM,
		BONE_TRANSFORM,
		BONE_MAP,	//!< instancingの際のBoneの参照先
		WORLD,
		ANIMATION_INFO,
		NUM,
	};
	struct InstancingResource
	{
		uint32_t m_instance_max_num;
		std::vector<MotionTexture> m_motion_texture;
		btr::AllocatedMemory m_compute_indirect_buffer;
		btr::AllocatedMemory m_world_staging;
		btr::AllocatedMemory m_instancing_info;
		std::array<btr::AllocatedMemory, static_cast<s32>(ModelStorageBuffer::NUM)> m_storage_buffer;
		const btr::AllocatedMemory& getBuffer(ModelStorageBuffer buffer)const { return m_storage_buffer[static_cast<s32>(buffer)]; }
		btr::AllocatedMemory& getBuffer(ModelStorageBuffer buffer) { return m_storage_buffer[static_cast<s32>(buffer)]; }

	};
	std::unique_ptr<InstancingResource> m_resource_instancing;

public:
	void setup(app::Loader* loader, std::shared_ptr<cModel::Resource> resource, uint32_t instanceNum);
	void addModel(cModel* model) { m_model.push_back(model); }

	void setup(cModelRenderer& renderer);
	void execute(cModelRenderer& renderer, vk::CommandBuffer& cmd);
	void draw(cModelRenderer& renderer, vk::CommandBuffer& cmd);

protected:
private:
};

struct LightSample : public Light
{
	LightParam m_param;
	int life;

	LightSample()
	{
		life = std::rand() % 50 + 30;
		m_param.m_position = glm::vec4(glm::ballRand(3000.f), std::rand() % 50 + 500.f);
		m_param.m_emission = glm::vec4(glm::normalize(glm::abs(glm::ballRand(1.f)) + glm::vec3(0.f, 0.f, 0.01f)), 1.f);

	}
	virtual bool update() override
	{
//		life--;
		return life >= 0;
	}

	virtual LightParam getParam()const override
	{
		return m_param;
	}

};

struct cModelRenderer
{
	cDevice m_device;
	cFowardPlusPipeline m_light_pipeline;
	cInstancingModelPipeline m_compute_pipeline;
	std::vector<ModelRender*> m_model;

public:
	cModelRenderer()
	{
		const cGPU& gpu = sThreadLocal::Order().m_gpu;
		auto device = gpu.getDevice(vk::QueueFlagBits::eGraphics)[0];
		m_device = device;
	}
	void setup(app::Loader& loader)
	{
		m_light_pipeline.setup(*this);
		m_compute_pipeline.setup(loader, *this);
	}
	void addModel(ModelRender* model)
	{
		m_model.emplace_back(model);
		m_model.back()->setup(*this);
	}
	void execute(vk::CommandBuffer cmd)
	{
		{
			auto* camera = cCamera::sCamera::Order().getCameraList()[0];
			CameraGPU2 cameraGPU;
			cameraGPU.setup(*camera);
			m_compute_pipeline.m_camera.subupdate(cameraGPU);
			m_compute_pipeline.m_camera.update(cmd);
		}

		m_light_pipeline.execute(cmd);


		for (auto& render : m_model)
		{
			render->execute(*this, cmd);
		}

	}
	void draw(vk::CommandBuffer cmd)
	{
		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_compute_pipeline.m_graphics_pipeline);
		// draw
		for (auto& render : m_model)
		{
			render->draw(*this, cmd);
		}

	}

	cInstancingModelPipeline& getComputePipeline() { return m_compute_pipeline; }
	cFowardPlusPipeline& getLight() {
		return m_light_pipeline;
	}
};
