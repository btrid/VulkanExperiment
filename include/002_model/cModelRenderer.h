#pragma once

#include <vector>
#include <list>
#include <btrlib/Define.h>
#include <btrlib/sGlobal.h>
#include <btrlib/cCamera.h>
#include <btrlib/Shape.h>

class Frustom {

public:
	glm::vec3 ntl_;	//!< nearTopLeft
	glm::vec3 ntr_;
	glm::vec3 nbl_;
	glm::vec3 nbr_;
	glm::vec3 ftl_;	//!< nearTopLeft
	glm::vec3 ftr_;
	glm::vec3 fbl_;
	glm::vec3 fbr_;

	float nearD_, farD_, ratio_, angle_, tang_;
	float nw_, nh_, fw_, fh_;

	enum {
		PLANE_TOP,
		PLANE_BOTTOM,
		PLANE_RIGHT,
		PLANE_LEFT,
		PLANE_NEAR,
		PLANE_FAR,
		PLANE_NUM,
	};
	std::array<Plane, 6> plane_;
public:

	std::array<Plane, 6>& getPlane() { return plane_; }
	Frustom()
	{}

	void setCamera(const cCamera& camera)
	{
		setProjection(camera.getFov(), camera.getAspect(), camera.getNear(), camera.getFar());
		setView(camera.getPosition(), camera.getTarget(), camera.getUp());
	}
	void setProjection(float angle, float ratio, float nearD, float farD)
	{
		// store the information
		ratio_ = ratio;
		angle_ = angle;
		nearD_ = nearD;
		farD_ = farD;

		// compute width and height of the near and far plane sections
		tang_ = tan(angle * 0.5f);
		nh_ = nearD * tang_;
		nw_ = nh_ * ratio;
		fh_ = farD  * tang_;
		fw_ = fh_ * ratio;
	}
	void setView(const glm::vec3 &p, const glm::vec3 &l, const glm::vec3 &u) {

		glm::vec3 Z = glm::normalize(p - l);
		glm::vec3 X = glm::normalize(glm::cross(u, Z));
		glm::vec3 Y = glm::normalize(glm::cross(Z, X));

		glm::vec3 nc = p - Z * nearD_;
		glm::vec3 fc = p - Z * farD_;

		// compute the 4 corners of the frustum on the near plane
		ntl_ = nc + Y * nh_ - X * nw_;
		ntr_ = nc + Y * nh_ + X * nw_;
		nbl_ = nc - Y * nh_ - X * nw_;
		nbr_ = nc - Y * nh_ + X * nw_;

		// compute the 4 corners of the frustum on the far plane
		ftl_ = fc + Y * fh_ - X * fw_;
		ftr_ = fc + Y * fh_ + X * fw_;
		fbl_ = fc - Y * fh_ - X * fw_;
		fbr_ = fc - Y * fh_ + X * fw_;

		// compute the six planes
		// the function set3Points assumes that the points
		// are given in counter clockwise order
		plane_[PLANE_TOP] = Plane(ntr_, ntl_, ftl_);
		plane_[PLANE_BOTTOM] = Plane(nbl_, nbr_, fbr_);
		plane_[PLANE_LEFT] = Plane(ntl_, nbl_, fbl_);
		plane_[PLANE_RIGHT] = Plane(nbr_, ntr_, fbr_);
		plane_[PLANE_NEAR] = Plane(ntl_, ntr_, nbr_);
		plane_[PLANE_FAR] = Plane(ftr_, ftl_, fbl_);
	}

	bool isInFrustom(const glm::vec3& p) {
		for (int i = 0; i < PLANE_NUM; i++) {
			if (plane_[i].getDistance(p) < 0.f) {
				return false;
			}
		}

		return true;
	}
	bool isInFrustom(const glm::vec3& p, float radius)
	{
		for (int i = 0; i < PLANE_NUM; i++) {
			if (plane_[i].getDistance(p) < radius) {
				return false;
			}
		}
		return true;
	}
};

template<typename T>
struct cModelRenderer_t
{
	struct Image
	{
		vk::Image image;
		vk::ImageView view;
		vk::Format format;
	};

	struct cModelDrawPipeline 
	{
		enum 
		{
			SHADER_RENDER_VERT,
			SHADER_RENDER_FRAG,
			SHADER_NUM,
		};
		std::array<vk::ShaderModule, SHADER_NUM> m_shader_list;
		std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

		vk::Pipeline m_pipeline;
		vk::PipelineLayout m_pipeline_layout;
		vk::DescriptorPool m_descriptor_pool;
		vk::DescriptorSet m_draw_descriptor_set_per_scene;
		enum {
			DESCRIPTOR_SET_LAYOUT_PER_MODEL,
			DESCRIPTOR_SET_LAYOUT_PER_MESH,
			DESCRIPTOR_SET_LAYOUT_PER_SCENE,
			DESCRIPTOR_SET_LAYOUT_MAX,
		};
		std::array<vk::DescriptorSetLayout, DESCRIPTOR_SET_LAYOUT_MAX> m_descriptor_set_layout;

		ConstantBuffer	m_camera_uniform;
		ConstantBuffer	m_camera_frustom;
		void setup(vk::RenderPass render_pass);
	};
	struct cModelComputePipeline {

		enum {
			SHADER_COMPUTE_CLEAR,
			SHADER_COMPUTE_ANIMATION_UPDATE,
			SHADER_COMPUTE_MOTION_UPDATE,
			SHADER_COMPUTE_NODE_TRANSFORM,
			SHADER_COMPUTE_CULLING,
			SHADER_COMPUTE_BONE_TRANSFORM,
			SHADER_NUM,
		};
		std::array<vk::ShaderModule, SHADER_NUM> m_shader_list;
		std::array<vk::PipelineShaderStageCreateInfo, SHADER_NUM> m_stage_info;

		vk::DescriptorPool m_descriptor_pool;
		vk::PipelineCache m_cache;
		std::vector<vk::Pipeline> m_pipeline;
		std::vector<vk::PipelineLayout> m_pipeline_layout;
		std::vector<vk::DescriptorSetLayout> m_descriptor_set_layout;

		void setup();
	};


protected:
	using RenderPtr = std::unique_ptr<T>;
// 	std::list<std::future<std::unique_ptr<T>>> m_loading_model;
// 	std::vector<std::unique_ptr<T>> m_render;
	cGPU	m_gpu;

	cModelDrawPipeline m_draw_pipeline;
	cModelComputePipeline m_compute_pipeline;
public:
	cModelRenderer_t();
	void setup(vk::RenderPass render_pass);
	void execute(cThreadPool& threadpool);

public:
	vk::CommandPool getCmdPool()const { return m_cmd_pool; }

	cModelDrawPipeline& getDrawPipeline() { return m_draw_pipeline; }
	cModelComputePipeline& getComputePipeline() { return m_compute_pipeline; }
};


#include "002_model/cModelRenderer.inl"

