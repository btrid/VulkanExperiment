#include "PhotonMap4.h"

#include <filesystem>
#include <vector>
#include <queue>
#include <memory>
#include <random>
#include <thread>
#include <future>
#include <bitset>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <ogldef.h>
#include "Renderer.h"
#include "DrawHelper.h"
#include "FrameBuffer.h"
#include "PhotonMap4Define.h"
#include "OctreeDefine.h"
#include "OctTree.h"
#include "Buffer.h"
namespace btr 
{
	struct Geometry{};
	
	struct InputAssembly
	{
		bool m_is_enable_primitive_restart;
		int m_primitive_restart_index;
		InputAssembly()
			: m_is_enable_primitive_restart(false)
			, m_primitive_restart_index(-1)
		{}

	};

}
namespace pm4
{

Mesh createMesh(int type)
{

	std::vector<glm::vec3> v;
	std::vector<unsigned> e;
	switch (type)
	{
	case 1:
		DrawHelper::GetSphere(v, e);
		break;
	case 2:
		DrawHelper::GetPlane(v, e);
		break;
	default:
		{
			DrawHelper::GetBox(v);
			e.resize(v.size());
			for (int i = 0; i < e.size(); i++) {
				e[i] = i;
			}
		}
		break;
	}
	auto n = DrawHelper::CalcNormal(v, e);
	auto tb = DrawHelper::createOrthoNormalBasis(n);

	Mesh mesh;
	mesh.mIndex.reserve(e.size() / 3);
	for (int i = 0; i < e.size(); i+=3)
	{
		mesh.mIndex.emplace_back(e[i], e[i + 1], e[i + 2]);
	}
	mesh.mVertex.resize(v.size());
	for (unsigned i = 0; i < v.size(); i++)
	{
		mesh.mVertex[i].mPosition = v[i];
		mesh.mVertex[i].mNormal = n[i];
// 			mesh.mVertex[i].mTangent = std::get<0>(tb)[i];
// 			mesh.mVertex[i].mBinormal = std::get<1>(tb)[i];
// 			mesh.mVertex[i].mTexcoord = glm::vec3(0.f);
		mesh.mVertex[i].mMaterialIndex = 0;
	}

	return mesh;
}
struct BrickParam
{
	glm::ivec3 size0;
	float p1;
	glm::ivec3 size1;
	int scale1;
	
	glm::vec3 areaMin;
	float p3;
	glm::vec3 areaMax;
	float p4;
};

void PhotonMap::Scene::push(const Model& model)
{
	mModelList.mData.push_back(model);
}
void PhotonMap::Scene::pushLight(const Model& model)
{
	mLightList.mData.push_back(model);
}

void PhotonMap::Scene::setup()
{
	mBuffer.setup(mModelList);
	mBuffer.setupLight(mLightList);


}

PhotonMap::PhotonMap()
{

//	glDebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_PERFORMANCE, GL_BUFFER, GL_DEBUG_SEVERITY_LOW, -1, "test\n");
	ShaderProgram::AddHeader("../GL/PhotonMap4Define.h", "/PMDefine.glsl");
	ShaderProgram::AddHeader("../GL/OctreeDefine.h", "/VoxelDefine.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM4/Voxel.glsl", "/Voxel.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM4/convert_vec4_to_int.glsl", "/convert_vec4_to_int.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM4/PM.glsl", "/PM.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM4/Shape.glsl", "/Shape.glsl");

	setupScene();

	int width = 640;
	int height = 480;

	Camera camera;
	{
		vec3 eye = vec3(0., 30., -230.);
		vec3 target = vec3(0., 10., 0.);
		camera.lookAt(eye, target, glm::vec3(0.f, 1.f, 0.f));
		camera.perspective(glm::radians(60.f), width, height, 0.1f, 100000.f);

	}
	Texture render;
	{
		std::vector<vec4> color(width * height);
		render.create(TextureTarget::TEXTURE_2D);
		render.image2D(0, InternalFormat::RGBA32F, width, height, Format::RGBA, PixelType::FLOAT);
	}

	ProgramPipeline brickRender;
	VertexProgram brickRenderVS;
	FragmentProgram brickRenderFS;
	brickRenderVS.createFromFile("../Resource/Shader/PM4/VoxelRender.vert");
	brickRenderFS.createFromFile("../Resource/Shader/PM4/VoxelRender.frag");
	brickRender.useProgramStage(brickRenderVS, ShaderBit::VERTEX_SHADER_BIT);
	brickRender.useProgramStage(brickRenderFS, ShaderBit::FRAGMENT_SHADER_BIT);

	unsigned BrickVAO;
	std::array<unsigned, 2> BrickVBO;
	{
		glCreateBuffers(BrickVBO.size(), BrickVBO.data());
		std::vector<glm::vec3> box;
		std::vector<glm::ivec3> index;
		std::tie(box, index) = DrawHelper::GetBox();
		glNamedBufferData(BrickVBO[0], sizeof(glm::ivec3)*index.size(), index.data(), GL_STATIC_DRAW);
		glNamedBufferData(BrickVBO[1], sizeof(glm::vec3)*box.size(), box.data(), GL_STATIC_DRAW);

		glCreateVertexArrays(1, &BrickVAO);
		glVertexArrayElementBuffer(BrickVAO, BrickVBO[0]);
		glVertexArrayVertexBuffer(BrickVAO, 0, BrickVBO[1], 0, sizeof(glm::vec3));
		glVertexArrayAttribBinding(BrickVAO, 0, 0);
		glEnableVertexArrayAttrib(BrickVAO, 0);
		glVertexArrayAttribFormat(BrickVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);
	}

	struct VoxelData
	{
		glm::ivec1 m_count;
		int _p[3];
		glm::vec4 m_albedo;
		glm::vec4 m_normal;
	};

	TreeInfo octreeInfo;
	octreeInfo.depth_ = VOXEL_DEPTH;
	octreeInfo.min_ = glm::BRICK_AREA_MIN;
	octreeInfo.max_ = glm::BRICK_AREA_MAX;
	auto octreedata =createOctree(octreeInfo);
// 	btr::Buffer<VoxelData> Voxel;
// 	{
// 		Voxel.create("VoxelBuffer");
// 		Voxel.allocate(BRICK_SUB_TOTAL, btr::BufferUsageFlag(), std::vector<VoxelData>());
// 	}
	std::array<unsigned, 4> VoxelTexture;
	{
		glCreateTextures(GL_TEXTURE_3D, VoxelTexture.size(), VoxelTexture.data());
//		glTextureImage3DEXT(VoxelTexture[0], GL_TEXTURE_3D, 0, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE, 0, Format::RED, PixelType::FLOAT, nullptr);
//		glTextureImage3DEXT(VoxelTexture[1], GL_TEXTURE_3D, 0, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE, 0, Format::RED, PixelType::FLOAT, nullptr);
//		glTextureImage3DEXT(VoxelTexture[2], GL_TEXTURE_3D, 0, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE, 0, Format::RED, PixelType::FLOAT, nullptr);
//		glTextureImage3DEXT(VoxelTexture[3], GL_TEXTURE_3D, 0, InternalFormat::R32UI, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE, 0, Format::RED_INTEGER, PixelType::UNSIGNED_INT, nullptr);
		glTextureStorage3D(VoxelTexture[0], VOXEL_DEPTH, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
		glTextureStorage3D(VoxelTexture[1], VOXEL_DEPTH, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
		glTextureStorage3D(VoxelTexture[2], VOXEL_DEPTH, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
		glTextureStorage3D(VoxelTexture[3], VOXEL_DEPTH, InternalFormat::R32UI, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE);

		glTextureParameteri(VoxelTexture[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelTexture[0], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelTexture[1], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelTexture[1], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelTexture[2], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelTexture[2], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelTexture[3], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelTexture[3], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	std::array<unsigned, 3> VoxelNormal;
	{
		glCreateTextures(GL_TEXTURE_3D, VoxelNormal.size(), VoxelNormal.data());
		glTextureStorage3D(VoxelNormal[0], VOXEL_DEPTH, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
		glTextureStorage3D(VoxelNormal[1], VOXEL_DEPTH, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
		glTextureStorage3D(VoxelNormal[2], VOXEL_DEPTH, InternalFormat::R32F, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
		glTextureParameteri(VoxelNormal[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelNormal[0], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelNormal[1], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelNormal[1], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelNormal[2], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTextureParameteri(VoxelNormal[2], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	ProgramPipeline voxel;
	VertexProgram voxelVS;
	GeometryProgram voxelGS;
	FragmentProgram voxelFS;
	voxelVS.createFromFile("../Resource/Shader/PM4/Voxelize.vert");
	voxelGS.createFromFile("../Resource/Shader/PM4/Voxelize.geom");
	voxelFS.createFromFile("../Resource/Shader/PM4/Voxelize.frag");
	voxel.useProgramStage(voxelVS, ShaderBit::VERTEX_SHADER_BIT);
	voxel.useProgramStage(voxelGS, ShaderBit::GEOMETRY_SHADER_BIT);
	voxel.useProgramStage(voxelFS, ShaderBit::FRAGMENT_SHADER_BIT);

	ProgramPipeline voxelHierarchy("VoxelMakeHierarchy");
	ComputeProgram voxelHierarchyCS;
	voxelHierarchyCS.createFromFile("../Resource/Shader/PM4/VoxelMakeHierarchy.comp");
	voxelHierarchy.useProgramStage(voxelHierarchyCS, ShaderBit::COMPUTE_SHADER_BIT);

	while (!Renderer::order()->isClose())
	{
		auto b = std::chrono::system_clock::now();

		camera.control(0.16f);


//		if(0)
		{
			// Voxel生成

			// テクスチャ初期化
			{
				std::uint32_t f[4] = { };
				glClearTexImage(VoxelTexture[0], 0, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelTexture[1], 0, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelTexture[2], 0, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelNormal[0], 0, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelNormal[1], 0, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelNormal[2], 0, Format::RED, PixelType::FLOAT, f);
				std::uint32_t c[4] = { 0x0, 0x0, 0x0, 0x0 };
				glClearTexImage(VoxelTexture[3], 0, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);

//				glClearNamedBufferData(Voxel.getHandle(), InternalFormat::RG32F, Format::RG, PixelType::FLOAT, f);
			}

			glViewport(0, 0, BRICK_SUB_SIZE, BRICK_SUB_SIZE);
			glDisable(GL_CULL_FACE);
			glDepthMask(GL_FALSE);
			glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_ALPHA_TEST);
			glClearColor(0.2f, 0.2f, 0.2f, 1.f);
			Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			voxel.bind();
			std::vector<unsigned> textures = {
				VoxelTexture[0],
				VoxelTexture[1],
				VoxelTexture[2],
				VoxelNormal[0],
				VoxelNormal[1],
				VoxelNormal[2],
				VoxelTexture[3],
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> buffers = {
				mScene.getBuffer().mBO[Buffer::BO_MATERIAL],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());


			mScene.drawScene();
		}
		// Octree作成
		if(0)
		{
			voxelHierarchy.bind();
			for (int i = 1; i < VOXEL_DEPTH; i++)
			{

				std::uint32_t f[4] = {};
				glClearTexImage(VoxelTexture[0], i, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelTexture[1], i, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelTexture[2], i, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelNormal[0], i, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelNormal[1], i, Format::RED, PixelType::FLOAT, f);
				glClearTexImage(VoxelNormal[2], i, Format::RED, PixelType::FLOAT, f);
				std::uint32_t c[4] = { 0x0, 0x0, 0x0, 0x0 };
				glClearTexImage(VoxelTexture[3], i, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
				glFinish();

				int bind = 0;
				glBindImageTexture(bind++, VoxelTexture[3], i-1, true, 0, GL_READ_ONLY, InternalFormat::R32UI);
				glBindImageTexture(bind++, VoxelTexture[0], i-1, true, 0, GL_READ_ONLY, InternalFormat::R32F);
				glBindImageTexture(bind++, VoxelTexture[1], i-1, true, 0, GL_READ_ONLY, InternalFormat::R32F);
				glBindImageTexture(bind++, VoxelTexture[2], i-1, true, 0, GL_READ_ONLY, InternalFormat::R32F);
//				glBindImageTexture(bind++, VoxelNormal[0], i-1, false, 0, GL_READ_ONLY, Format::RED);
//				glBindImageTexture(bind++, VoxelNormal[1], i-1, false, 0, GL_READ_ONLY, Format::RED);
//				glBindImageTexture(bind++, VoxelNormal[2], i-1, false, 0, GL_READ_ONLY, Format::RED);
				glBindImageTexture(bind++, VoxelTexture[3], i, true, 0, GL_READ_WRITE, InternalFormat::R32UI);
				glBindImageTexture(bind++, VoxelTexture[0], i, true, 0, GL_READ_WRITE, InternalFormat::R32F);
				glBindImageTexture(bind++, VoxelTexture[1], i, true, 0, GL_READ_WRITE, InternalFormat::R32F);
				glBindImageTexture(bind++, VoxelTexture[2], i, true, 0, GL_READ_WRITE, InternalFormat::R32F);
//				glBindImageTexture(bind++, VoxelNormal[0], i, false, 0, GL_READ_WRITE, Format::RED);
//				glBindImageTexture(bind++, VoxelNormal[1], i, false, 0, GL_READ_WRITE, Format::RED);
//				glBindImageTexture(bind++, VoxelNormal[2], i, false, 0, GL_READ_WRITE, Format::RED);

				voxelHierarchyCS.setUniform("uHierarchy", VOXEL_DEPTH-i);
				int num =OctreeCellNum[VOXEL_DEPTH - i]/8;
				num = glm::max(num / 8, 1);
				glDispatchCompute(num, num, num);
				glFinish();
			}
		}
		// 画面からの光線
#define VOXEL_TEST 1
#if VOXEL_TEST==0

//		if(0)
		{
			photonRendering.bind();

			std::vector<unsigned> textures = {
				BrickMap0.getHandle(),
				render.getHandle(),
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				mScene.getBuffer().mBO[Buffer::BO_VERTEX],
				mScene.getBuffer().mBO[Buffer::BO_ELEMENT],
				mScene.getBuffer().mBO[Buffer::BO_MATERIAL],
				triangleBO[TBO_LL_HEAD],
				triangleBO[TBO_LL],
				photonBO[PBO_DATA],
				photonBO[PBO_LL_HEAD],
				photonBO[PBO_LL],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());


			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBO[IBO_RENDERING]);
			glDispatchComputeIndirect(0);

			photonRenderingCS.setUniform("eye", camera.getPosition());
			photonRenderingCS.setUniform("target", camera.getTarget());

			// 描画
			{
				glViewport(0, 0, width, height);
				glDepthMask(GL_FALSE);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				DrawHelper::Order()->drawTexture(render.getHandle());
			}
		}
#else
		{
			int level = 0;
			glViewport(0, 0, width, height);
			glDepthMask(GL_TRUE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glEnable(GL_ALPHA_TEST);
			glEnable(GL_DEPTH_TEST);
			Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			brickRender.bind();
			glBindVertexArray(BrickVAO);

			int bind = 0;
			glBindImageTexture(bind++, VoxelTexture[0], level, true, 0, GL_READ_ONLY, InternalFormat::R32F);
			glBindImageTexture(bind++, VoxelTexture[1], level, true, 0, GL_READ_ONLY, InternalFormat::R32F);
			glBindImageTexture(bind++, VoxelTexture[2], level, true, 0, GL_READ_ONLY, InternalFormat::R32F);
			glBindImageTexture(bind++, VoxelTexture[3], level, true, 0, GL_READ_ONLY, InternalFormat::R32UI);


			brickRenderVS.setUniform("PV", camera.getProjection()*camera.getView());
			brickRenderVS.setUniform("uHierarchy", VOXEL_DEPTH - level);
			int num = OctreeCellNum[VOXEL_DEPTH - level];
			glDrawElementsInstanced(GL_LINES, 36, GL_UNSIGNED_INT, nullptr, num + 1);
//			glDrawElementsInstanced(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr, BRICK_SUB_SIZE*BRICK_SUB_SIZE*BRICK_SUB_SIZE);
		}
#endif


		Renderer::order()->swapBuffer();
		Renderer::order()->loopEvent();

		auto e = std::chrono::system_clock::now();
		printf("%lld.%03lld\n", std::chrono::duration_cast<std::chrono::seconds>(e - b).count(), std::chrono::duration_cast<std::chrono::milliseconds>(e - b).count()%1000);
	}

}

void PhotonMap::setupScene()
{
// 	if (0)
	{
//		unsigned modelFlag = aiProcessPreset_TargetRealtime_MaxQuality
		unsigned modelFlag = 0
			| aiProcess_JoinIdenticalVertices
			| aiProcess_ImproveCacheLocality
			| aiProcess_LimitBoneWeights
			| aiProcess_RemoveRedundantMaterials
//			| aiProcess_SplitLargeMeshes
			| aiProcess_Triangulate
			| aiProcess_SortByPType
			| aiProcess_FindDegenerates
//			| aiProcess_CalcTangentSpace
//				| aiProcess_FlipUVs
			;

		using namespace Assimp;
		Assimp::Importer importer;
		std::string filepath = "../Resource/Tiny/tiny.x";
		importer.ReadFile(filepath, modelFlag);
		const aiScene* scene = importer.GetScene();

		Mesh mesh;
		std::vector<Material> material(scene->mNumMaterials);
		{
			std::string path = std::tr2::sys::path(filepath).remove_filename().string();
			for (size_t i = 0; i < scene->mNumMaterials; i++)
			{
				auto* aiMat = scene->mMaterials[i];
				auto& mat = material[i];
				aiColor4D color;
				aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
				std::memcpy(&mat.mDiffuse, &color, sizeof(color));
				aiMat->Get(AI_MATKEY_COLOR_AMBIENT, color);
				//				std::memcpy(&mat.mAmbient, &color, sizeof(color));
				aiMat->Get(AI_MATKEY_COLOR_SPECULAR, color);
				//				std::memcpy(&mat.mSpecular, &color, sizeof(color));
				aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, color);
				//				std::memcpy(&mat.mEmissive, &color, sizeof(color));
								//				aiMat->Get(AI_MATKEY_SHININESS, mat.mShininess);
				aiString str;
				aiTextureMapMode mapmode[3];
				aiTextureMapping mapping;
				unsigned uvIndex;
				if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
					glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					mat.m_diffuse_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_diffuse_tex);
				}
				if (aiMat->GetTexture(aiTextureType_AMBIENT, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					mat.m_ambient_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_ambient_tex);
				}
				if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					mat.m_specular_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_specular_tex);
				}
				if (aiMat->GetTexture(aiTextureType_EMISSIVE, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode) == AI_SUCCESS) {
					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
					uint32_t tex;
					glCreateTextures(GL_TEXTURE_2D, 1, &tex);
					glTextureImage2DEXT(tex, GL_TEXTURE_2D, 0, InternalFormat::RGBA32F, data.mSize.x, data.mSize.y, 0, Format::RGBA, PixelType::FLOAT, data.mData.data());
					mat.m_emissive_tex = glGetTextureHandleARB(tex);
					glMakeTextureHandleResidentARB(mat.m_emissive_tex);
				}

				//				if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &str, &mapping, &uvIndex, NULL, NULL, mapmode)) {
				// 					Texture::Data data = Texture::LoadTextureEx(path + "/" + str.C_Str());
				// 					mat.mTextureNormal.mData = std::make_shared<std::vector<glm::vec4>>(data.mData);
				// 					mat.mTextureNormal.mSize = data.mSize;
				//				}

			}

		}

		{

			unsigned numIndex = 0;
			unsigned numVertex = 0;
			std::vector<int> _vertexSize(scene->mNumMeshes);
			std::vector<int> _materialIndex(scene->mNumMeshes);
			for (size_t i = 0; i < scene->mNumMeshes; i++)
			{
				numVertex += scene->mMeshes[i]->mNumVertices;
				numIndex += scene->mMeshes[i]->mNumFaces;
				_vertexSize[i] = scene->mMeshes[i]->mNumVertices;
			}

			std::vector<Vertex> _vertex;
			_vertex.reserve(numVertex);
			std::vector<glm::ivec3> _index;
			_index.reserve(numIndex);


			for (size_t i = 0; i < scene->mNumMeshes; i++)
			{
				aiMesh* mesh = scene->mMeshes[i];
				_materialIndex[i] = mesh->mMaterialIndex;

				// ELEMENT_ARRAY_BUFFER
				// 三角メッシュとして読み込む
				unsigned offset = _vertex.size();
				for (size_t n = 0; n < mesh->mNumFaces; n++) {
					_index.emplace_back(mesh->mFaces[n].mIndices[0] + offset, mesh->mFaces[n].mIndices[1] + offset, mesh->mFaces[n].mIndices[2] + offset);
				}

				// ARRAY_BUFFER
				std::vector<Vertex> vertex(mesh->mNumVertices);
				for (size_t i = 0; i < mesh->mNumVertices; i++)
				{
					vertex[i].mPosition = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
				}
				for (size_t i = 0; i < mesh->mNumVertices; i++)
				{
					vertex[i].mMaterialIndex = mesh->mMaterialIndex;
				}
				if (mesh->HasNormals()) {
					for (size_t i = 0; i < mesh->mNumVertices; i++)
					{
						vertex[i].mNormal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
					}
				}

				if (mesh->HasTextureCoords(0)) 
				{
					for (size_t i = 0; i < mesh->mNumVertices; i++)
					{
						vertex[i].m_texcoord = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
					}
				}
				if (mesh->HasTangentsAndBitangents())
				{
					// 						vertex[i].mTangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
					// 						vertex[i].mBinormal = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
				}
				_vertex.insert(_vertex.end(), vertex.begin(), vertex.end());
			}
			mesh.mVertex = _vertex;
			mesh.mIndex = _index;
			//				mesh.mMateria = mesh->mMaterialIndex;
		}


		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](auto& v) { v.mPosition.z += 50.f;  v.mPosition *= 0.4f; });

		Model model;
		model.mMesh.push_back(mesh);
		model.mMaterial = material;
		model.mWorld = glm::translate(glm::vec3(0.f, -900.f, 0.f));
		mScene.push(model);
	}

	// ライト
	if(0)
	{
		Mesh mesh = createMesh(2);
		std::swap(mesh.mIndex[0][1], mesh.mIndex[0][2]);
		std::swap(mesh.mIndex[1][1], mesh.mIndex[1][2]);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition *= 50.f; p.mPosition.y = 88.f; p.mNormal = -p.mNormal; });
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mEmissive = glm::vec4(1.0f);
		material[0].mDiffuse = glm::vec4(1.0f);

		model.mWorld *= glm::translate(glm::vec3(0.f, 19.9f, 0.f));
		model.mWorld *= glm::rotate(glm::radians(180.f), glm::vec3(0.f, 0.f, 1.f));
		model.mWorld *= glm::scale(glm::vec3(6.f, 1.f, 6.f));

		mScene.push(model);
		mScene.pushLight(model);

	}

	if (0)
	{
		// 中央のボックス
		Mesh mesh = createMesh(0);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition += glm::vec3(0.f, 0.5f, 0.f); p.mPosition *= 50.f; p.mPosition -= glm::vec3(0.f, 100.f, 0.f); });

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.0f, 0.f, 1.f, 1.f);

		mScene.push(model);

	}
	if (0)
	{
		Mesh mesh = createMesh(0);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition += glm::vec3(-5.f, 1.5f, 5.f); p.mPosition *= 60.f; });
		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 1.f, 0.f, 1.f);

		mScene.push(model);

	}

	if (0)
	{
		// 鏡
		Mesh mesh = createMesh(2);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition *= 150.f; });

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 0.f, 0.f, 1.f);
		material[0].m_diffuse = 0.f;
		material[0].m_specular = 1.f;


		mScene.push(model);

	}
	if (0)
	{
		// 鏡
		Mesh mesh = createMesh(2);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition *= 150.f; });

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(0.f, 0.f, 0.f, 1.f);
		material[0].m_diffuse = 0.f;
		material[0].m_specular = 1.f;


		mScene.push(model);

	}


	// 箱
//	if (0)
	{
		Model model;
		std::vector<glm::vec3> v;
		DrawHelper::GetBox(v);
		for (int i = 0; i < v.size()/3; i++)
		{
			std::swap(v[i * 3 + 1], v[i * 3 + 2]);
		}
		std::vector<unsigned> _i(v.size());
		for (int i = 0; i < _i.size(); i++) {
			_i[i] = i;
		}
		auto n = DrawHelper::CalcNormal(v, _i);
		std::for_each(v.begin(), v.end(), [](vec3& p) {p *= vec3(190.f); });

		Mesh mesh;

		mesh.mVertex.resize(v.size() - 6);
		for (unsigned i = 0; i < v.size() - 6; i++)
		{
			mesh.mVertex[i].mPosition = v[i];
			mesh.mVertex[i].mNormal = n[i];
			mesh.mVertex[i].mMaterialIndex = i / 6;
		}

		std::vector<glm::ivec3> e((v.size() - 6) / 3);
		for (int i = 0; i < e.size() * 3; i++) {
			e[i / 3][i % 3] = _i[i];
		}
		mesh.mIndex = e;


		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(6);
		int index = 0;
		material[index++].mDiffuse = glm::vec4(1.0f, 0.2f, 0.2f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.2f, 1.0f, 0.2f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.2f, 0.2f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(1.0f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.0f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);

		mScene.push(model);

	}

	mScene.setup();


}


}
