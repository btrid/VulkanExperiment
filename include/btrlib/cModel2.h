#pragma once

#include "cModel.h"

struct cModel2
{
	enum 
	{
		BONE_BLEND_MAX = 4,
	};

	struct Material {
		glm::vec4		mAmbient;
		glm::vec4		mDiffuse;
		glm::vec4		mSpecular;
		ResourceTexture	mDiffuseTex;
		ResourceTexture	mAmbientTex;
		ResourceTexture	mNormalTex;
		ResourceTexture	mSpecularTex;
		ResourceTexture	mHeightTex;
		ResourceTexture	mReflectionTex;
		float			mShininess;
		glm::vec4		mEmissive;
	};


	struct Mesh
	{
		vk::DrawIndexedIndirectCommand m_draw_cmd;
		int32_t mBufferOffset[16];
		s32 m_vertex_num;
		s32 m_node_index;	//!< 
		s32 m_material_index;
		glm::vec4 mAABB;	// xyz: center w:radius

		Mesh()
			: m_vertex_num(0)
			, m_material_index(-1)
			, m_node_index(-1)
		{}
	};


	struct Bone
	{
		std::string mName;
		glm::mat4 mOffset;
		int mNodeIndex;

		Bone()
			: mNodeIndex(-1)
		{}
		bool nameCompare(const std::string& name) {
			return mName.compare(name) == 0;
		}
	};
	struct ModelInfo 
	{
		s32 mNodeNum;
		s32 mBoneNum;
		s32 mMeshNum;
		s32 m_node_depth_max;

		glm::vec4 mAabb;
		glm::mat4 mInvGlobalMatrix;
	};

	std::string m_filename;

	ModelInfo m_model_info;
	RootNode mNodeRoot;
	std::vector<Bone> mBone;
	std::vector<Mesh> m_mesh;
	std::vector<Material> mMaterial;

	ResourceVertex m_mesh_resource;
	cAnimation m_animation;
};

std::shared_ptr<cModel2> load(const std::shared_ptr<btr::Context>& context, const std::string& filename);
