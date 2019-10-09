#include "PhotonMap2.h"

//#define _ITERATOR_DEBUG_LEVEL (0)
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
#include "PhotonMap2Define.h"

namespace pm2
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

	ShaderProgram::AddHeader("../GL/PhotonMap2Define.h", "/PMDefine.glsl"); //やめる
	ShaderProgram::AddHeader("../Resource/Shader/PM2/Brick.glsl", "/Brick.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM2/PM.glsl", "/PM.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM2/Shape.glsl", "/Shape.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM2/Marching.glsl", "/Marching.glsl");
	ShaderProgram::AddHeader("../Resource/Shader/PM2/StorePhotonBatch.glsl", "/StorePhotonBatch.glsl");

	setupScene();

	int width = 640;
	int height = 480;

	Camera camera;
	{
		vec3 eye = vec3(0., 30., -230.);
		vec3 target = vec3(0., 10., 0.);
		camera.lookAt(eye, target, glm::vec3(0.f, 1.f, 0.f));
		camera.perspective(glm::radians(60.f), 640.f, 480.f, 0.1f, 100000.f);

	}
	Texture render;
	{
		std::vector<vec4> color(width * height);
		render.create(TextureTarget::TEXTURE_2D);
		render.image2D(0, InternalFormat::RGBA32F, width, height, Format::RGBA, PixelType::FLOAT);
	}

	unsigned BrickBO;
	{
		BrickParam p;
		p.size0 = glm::ivec3(BRICK_SIZE);
		p.size1 = glm::ivec3(BRICK_SUB_SIZE);
		p.scale1 = BRICK_SCALE1;
		p.areaMin = glm::vec3(-100.f);
		p.areaMax = glm::vec3(100.f);

		glCreateBuffers(1, &BrickBO);
		glNamedBufferData(BrickBO, sizeof(BrickParam), &p, GL_STATIC_COPY);

	}


	ProgramPipeline brickRender;
	VertexProgram brickRenderVS;
	FragmentProgram brickRenderFS;
	brickRenderVS.createFromFile("../Resource/Shader/PM2/BrickRender.vert");
	brickRenderFS.createFromFile("../Resource/Shader/PM2/BrickRender.frag");
	brickRender.useProgramStage(brickRenderVS, ShaderBit::VERTEX_SHADER_BIT);
	brickRender.useProgramStage(brickRenderFS, ShaderBit::FRAGMENT_SHADER_BIT);

	unsigned BrickVAO;
	unsigned BrickVBO;
	{
		glCreateBuffers(1, &BrickVBO);
		std::vector<glm::vec3> box;
		DrawHelper::GetBox(box);
		glNamedBufferData(BrickVBO, sizeof(glm::vec3)*box.size(), box.data(), GL_STATIC_DRAW);

		glCreateVertexArrays(1, &BrickVAO);
		glVertexArrayVertexBuffer(BrickVAO, 0, BrickVBO, 0, sizeof(glm::vec3));
		glVertexArrayAttribBinding(BrickVAO, 0, 0);
		glEnableVertexArrayAttrib(BrickVAO, 0);
		glVertexArrayAttribFormat(BrickVAO, 0, 3, GL_FLOAT, GL_FALSE, 0);

	}
	Texture BrickMap0;
	Texture BrickMap1;
	{
		glm::ivec3 brickSize(BRICK_SIZE);
		BrickMap0.create(TextureTarget::TEXTURE_3D);
		BrickMap0.image3D(0, InternalFormat::R32UI, brickSize.x, brickSize.y, brickSize.z, Format::RED_INTEGER, PixelType::UNSIGNED_INT, nullptr);
	}

	{
		BrickMap1.create(TextureTarget::TEXTURE_3D);
#ifdef TILED_MAP
		BrickMap1.image3D(0, InternalFormat::R32UI, BRICK_SIZE, BRICK_SIZE, BRICK_SIZE*BRICK_SCALE1*BRICK_SCALE1*BRICK_SCALE1, Format::RED_INTEGER, PixelType::UNSIGNED_INT, nullptr);
#else
		BrickMap1.image3D(0, InternalFormat::R32UI, BRICK_SUB_SIZE, BRICK_SUB_SIZE, BRICK_SUB_SIZE, Format::RED_INTEGER, PixelType::UNSIGNED_INT, nullptr);
#endif
	}

	enum TriangleBO {
		TBO_REFERENCE_COUNT,
		TBO_REFERENCE_DATA,
		TBO_ATOMIC,
		TBO_NUM,
	};
	struct _TriangleLL
	{
		std::int32_t next;
		std::int32_t drawID;
		std::int32_t instanceID;
		std::int32_t _p1;

		std::int32_t index[3];
		std::int32_t _p2;
	};
	using TriangleLL = _TriangleLL;
	std::array<unsigned, TBO_NUM> triangleBO;
	glCreateBuffers(triangleBO.size(), triangleBO.data());
	glNamedBufferData(triangleBO[TBO_REFERENCE_COUNT], sizeof(std::int32_t) *BRICK_SUB_TOTAL, nullptr, GL_STREAM_COPY);
	glNamedBufferData(triangleBO[TBO_REFERENCE_DATA], sizeof(TriangleLL)  * TRIANGLE_BLOCK_NUM*BRICK_SUB_TOTAL, nullptr, GL_STREAM_COPY);
	glNamedBufferData(triangleBO[TBO_ATOMIC], sizeof(std::uint32_t), nullptr, GL_STREAM_COPY);

	ProgramPipeline triangleClear;
	ComputeProgram triangleClearCS;
	triangleClearCS.createFromFile("../Resource/Shader/PM2/TriangleClear.comp");
	triangleClear.useProgramStage(triangleClearCS, ShaderBit::COMPUTE_SHADER_BIT);

	ProgramPipeline voxel;
	VertexProgram voxelVS;
	GeometryProgram voxelGS;
	FragmentProgram voxelFS;
	voxelVS.createFromFile("../Resource/Shader/PM2/Voxelize.vert");
	voxelGS.createFromFile("../Resource/Shader/PM2/Voxelize.geom");
	voxelFS.createFromFile("../Resource/Shader/PM2/Voxelize.frag");
	voxel.useProgramStage(voxelVS, ShaderBit::VERTEX_SHADER_BIT);
	voxel.useProgramStage(voxelGS, ShaderBit::GEOMETRY_SHADER_BIT);
	voxel.useProgramStage(voxelFS, ShaderBit::FRAGMENT_SHADER_BIT);

	unsigned PhotonLL;
	glCreateBuffers(1, &PhotonLL);

	enum PhotonBO{
		PBO_LL_HEAD,
		PBO_LL,
		PBO_DATA,
		PBO_DATA_COUNT,
		PBO_EMIT_DATA,
		PBO_EMIT_BLOCK_COUNT,
		PBO_EMIT_BATCH_DATA,
		PBO_EMIT_BATCH_DATA_COUNT,
		PBO_MARCH_PROCESS_INDEX,
		PBO_NUM,
	};

	unsigned PhotonNumBuffer;
	{
		glCreateBuffers(1, &PhotonNumBuffer);
		std::vector<int> b(BRICK_TOTAL);
		glNamedBufferData(PhotonNumBuffer, 4 * BRICK_TOTAL, b.data(), GL_STREAM_COPY);
	}
	std::array<unsigned, PBO_NUM> photonBO;
	glCreateBuffers(photonBO.size(), photonBO.data());
	glNamedBufferData(photonBO[PBO_DATA_COUNT], sizeof(std::int32_t), nullptr, GL_STREAM_COPY);
	glNamedBufferData(photonBO[PBO_LL_HEAD], sizeof(std::uint32_t) * BRICK_SUB_TOTAL, nullptr, GL_STREAM_COPY);
	glNamedBufferData(photonBO[PBO_LL], sizeof(std::uint32_t) * 1000 * 1000 * 10, nullptr, GL_STREAM_COPY);
	glNamedBufferData(photonBO[PBO_DATA], sizeof(Photon) * 1000 * 1000 * 10, nullptr, GL_STREAM_COPY);

	glNamedBufferData(photonBO[PBO_EMIT_DATA], sizeof(Photon) * PHOTON_EMIT_BUFFER_SIZE, nullptr, GL_DYNAMIC_COPY);
	glNamedBufferData(photonBO[PBO_EMIT_BLOCK_COUNT], sizeof(std::uint32_t) * 1, nullptr, GL_DYNAMIC_COPY);
	glNamedBufferData(photonBO[PBO_MARCH_PROCESS_INDEX], sizeof(std::uint32_t) * 1, nullptr, GL_DYNAMIC_COPY);

	glNamedBufferData(photonBO[PBO_EMIT_BATCH_DATA], sizeof(Photon) * BRICK_TOTAL * PHOTON_BATCH_BUFFER_SIZE, nullptr, GL_DYNAMIC_COPY);
	glNamedBufferData(photonBO[PBO_EMIT_BATCH_DATA_COUNT], sizeof(glm::ivec2)* BRICK_TOTAL, nullptr, GL_DYNAMIC_COPY);

	ProgramPipeline photonMake;
	ComputeProgram photonMakeCS;
	photonMakeCS.createFromFile("../Resource/Shader/PM2/PhotonMake.comp");
	photonMake.useProgramStage(photonMakeCS, ShaderBit::COMPUTE_SHADER_BIT);

	ProgramPipeline photonMapping;
	ComputeProgram photonMappingCS;
	photonMappingCS.createFromFile("../Resource/Shader/PM2/PhotonMapping.comp");
	photonMapping.useProgramStage(photonMappingCS, ShaderBit::COMPUTE_SHADER_BIT);

	ProgramPipeline photonRendering;
	ComputeProgram photonRenderingCS;
//	photonRenderingCS.createFromFile("../Resource/Shader/PM2/PhotonRendering.comp");
//	photonRendering.useProgramStage(photonRenderingCS, ShaderBit::COMPUTE_SHADER_BIT);

	enum IndirectBO {
		IBO_MAKE,
		IBO_MAPPING,
		IBO_RENDERING,
		IBO_MAX,
	};

	std::array<unsigned, IBO_MAX> indirectBO;
	{
		glCreateBuffers(indirectBO.size(), indirectBO.data());
		{
			glm::ivec3 ibo{ 64 , 1, 1 };
			glNamedBufferData(indirectBO[IBO_MAKE], sizeof(glm::ivec3), &ibo, GL_STATIC_DRAW);
		}
		{
			glm::ivec3 ibo{ BRICK_SUB_SIZE / 64, BRICK_SUB_SIZE / 16, BRICK_SUB_SIZE };
			glNamedBufferData(indirectBO[IBO_MAPPING], sizeof(glm::ivec3), &ibo, GL_STATIC_DRAW);
		}
		{
			glm::ivec3 ibo{ width/32, height/32, 1 };
			glNamedBufferData(indirectBO[IBO_RENDERING], sizeof(glm::ivec3), &ibo, GL_STATIC_DRAW);
		}
	}

	while (!Renderer::order()->isClose())
	{
		auto b = std::chrono::system_clock::now();

		camera.control(0.16f);

		// テクスチャ初期化
		{
			// シェーダで初期化
			{
				std::uint32_t c0[4] = { 0 };
				std::uint32_t cFF[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, };
				glClearTexImage(BrickMap0.getHandle(), 0, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c0);

				glClearNamedBufferData(PhotonNumBuffer, InternalFormat::R32I, Format::RED_INTEGER, PixelType::INT, c0);
			}
			{
				std::uint32_t c[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
				glClearTexImage(BrickMap1.getHandle(), 0, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
			}
			{
				// 一定以上bufferが大きいと実行時エラー？
#if defined(USE_SHADER) || 1
// 
				triangleClear.bind();
				{
					std::vector<unsigned> buffers = {
						photonBO[PBO_LL_HEAD],
					};
					glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());
					std::vector<unsigned> uniforms = {
						BrickBO,
					};
					glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());
					glDispatchCompute(BRICK_SUB_SIZE / 64, BRICK_SUB_SIZE / 16, BRICK_SUB_SIZE);
				}


#else
				std::uint32_t c[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
				glClearNamedBufferData(photonBO[PBO_LL_HEAD], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
				glClearNamedBufferData(photonBO[PBO_LL], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, nullptr);
				glClearNamedBufferData(triangleBO[TBO_REFERENCE_DATA], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
#endif
			}
 			std::uint32_t c4[16] = {};

			std::vector<std::int32_t> d(BRICK_SUB_SIZE*BRICK_SUB_SIZE*BRICK_SUB_SIZE);
			glNamedBufferSubData(triangleBO[TBO_REFERENCE_COUNT], 0, sizeof(int32_t)*d.size(), d.data());
//			glClearNamedBufferData(triangleBO[TBO_REFERENCE_NUM], InternalFormat::R32I, Format::RED_INTEGER, PixelType::INT, c4);
			std::vector<glm::ivec2> dd(BRICK_SIZE*BRICK_SIZE*BRICK_SIZE);
			glNamedBufferSubData(photonBO[PBO_EMIT_BATCH_DATA_COUNT], 0, sizeof(decltype(dd)::value_type)*dd.size(), dd.data());
//			glClearNamedBufferData(photonBO[PBO_EMIT_BATCH_DATA_COUNT], InternalFormat::RG32I, Format::RG_INTEGER, PixelType::INT, c4);

			std::uint32_t c[1] = { 0x0 };
			glClearNamedBufferData(photonBO[PBO_DATA_COUNT], InternalFormat::R32I, Format::RED_INTEGER, PixelType::INT, c);
			glClearNamedBufferData(triangleBO[TBO_ATOMIC], InternalFormat::R32I, Format::RED_INTEGER, PixelType::INT, c);
			glClearNamedBufferData(photonBO[PBO_EMIT_BLOCK_COUNT], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);
			glClearNamedBufferData(photonBO[PBO_MARCH_PROCESS_INDEX], InternalFormat::R32UI, Format::RED_INTEGER, PixelType::UNSIGNED_INT, c);

		}
//		if(0)
		{
			// Voxel生成
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
				BrickMap0.getHandle(),
				BrickMap1.getHandle(),
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				triangleBO[TBO_REFERENCE_COUNT],
				triangleBO[TBO_REFERENCE_DATA],
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());

			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, triangleBO[TBO_ATOMIC]);

			mScene.drawScene();

		}
		
#pragma region check_voxelize

		auto c1D3D = [](int in, glm::ivec3 size)->auto
		{
			int x = in % size.x;
			int y = (in / size.x) % size.y;
			int z = in / (size.y * size.x);

			return glm::ivec3(x, y, z);
		};
		if (0)
		{
			std::vector<char> a(BRICK_SIZE * BRICK_SIZE * BRICK_SIZE);
			glGetTextureImage(BrickMap0.getHandle(), 0, Format::RED_INTEGER, PixelType::UNSIGNED_BYTE, a.size() * 1, a.data());
			printf("Brick0\n");

			for (int i = 0; i < a.size(); i++)
			{
				if (a.data()[i] != 0)
				{
					auto vec = c1D3D(i, glm::ivec3(BRICK_SIZE));
					printf("[%2d,%2d,%2d] = %d\n", vec.x, vec.y, vec.z, a.data()[i]);
				}
			}
		}
		if (0)
		{
			// 
			glFinish();
			std::vector<int> a(BRICK_SUB_SIZE * BRICK_SUB_SIZE * BRICK_SUB_SIZE);
			glGetTextureImage(BrickMap1.getHandle(), 0, Format::RED_INTEGER, PixelType::UNSIGNED_INT, a.size() * 4, a.data());
			printf("Brick1\n");
			for (int i = 0; i < a.size(); i++)
			{
				if (a.data()[i] != -1)
				{
					auto vec = c1D3D(i, glm::ivec3(BRICK_SUB_SIZE));
					printf("[%3d,%3d,%3d] = %d\n", vec.x, vec.y, vec.z, a.data()[i]);
				}
			}
		}
		if(0)
		{
			glFinish();
			std::int32_t a = 0;
			glGetNamedBufferSubData(triangleBO[TBO_ATOMIC], 0, 4, &a);
			std::vector<TriangleLL> data(a);
			glGetNamedBufferSubData(triangleBO[TBO_REFERENCE_DATA], 0, data.size() * sizeof(TriangleLL), data.data());
			printf("triangleBO[TBO_LL]\n");
			for (int i = 0; i < data.size(); i++)
			{				
				auto& aaa = data.data()[i];
//				printf("[i=%5d]next=%5d, draw=%5d, instance=%5d, v[0, 1, 2]=%5d, %5d, %5d\n", i, aaa.next, aaa.drawID, aaa.instanceID, aaa.index[0], aaa.index[1], aaa.index[2]);
			}

		}
		if (0)
		{
			std::vector<int32_t> num(BRICK_SUB_TOTAL);
			glGetNamedBufferSubData(triangleBO[TBO_REFERENCE_COUNT], 0, num.size() * sizeof(int32_t), num.data());
			std::vector<TriangleLL> data(BRICK_SUB_TOTAL*TRIANGLE_BLOCK_NUM);
			glGetNamedBufferSubData(triangleBO[TBO_REFERENCE_DATA], 0, data.size() * sizeof(TriangleLL), data.data());
			int _i = 0;
			for (auto& n : num)
			{
				if (n  != 0)
				{
					printf("[i = %4d]num = %d\n", _i, n);
					for (size_t i = 0; i < n; i++)
					{
						auto& t = data.data()[_i*TRIANGLE_BLOCK_NUM + i];
						printf(" [i=%d]next=%5d, draw=%5d, instance=%5d, v[0, 1, 2]=%5d, %5d, %5d\n", i, t.next, t.drawID, t.instanceID, t.index[0], t.index[1], t.index[2]);
					}
				}
				_i++;
			}

		}

		if (0)
		{
			std::int32_t a = 0;
			std::int32_t b = 0;
			glGetNamedBufferSubData(triangleBO[TBO_ATOMIC], 0, 4, &a);
			glGetNamedBufferSubData(photonBO[PBO_DATA_COUNT], 0, 4, &b);
			printf("TriangleLL Num = %d, PhotonLL Num = %d\n", a, b);
		}
#pragma endregion
//		if(0)
		// 光源からフォトン生成
		{
			glFinish();
			photonMake.bind();

// 			std::vector<unsigned> textures = {
// 			};
// 			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			std::vector<unsigned> buffers = {
				mScene.getBuffer().mBO[Buffer::BO_LS_VERTEX],
				mScene.getBuffer().mBO[Buffer::BO_LS_ELEMENT],
				mScene.getBuffer().mBO[Buffer::BO_LS_MATERIAL],
				photonBO[PBO_EMIT_DATA],
				PhotonNumBuffer,
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());

			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, photonBO[PBO_EMIT_BLOCK_COUNT]);

//			glBindBuffer(GL_DISPATCH_INDIRECT_BUFFER, indirectBO[IBO_MAKE]);
//			glDispatchComputeIndirect(0);

			glFinish();
			glDispatchCompute(1024, 1, 1);
			glFinish();
		}

#pragma region check_make_photon
		if (0)
		{
			std::vector<Photon> a(1 * PHOTON_BLOCK_NUM);
			glGetNamedBufferSubData(photonBO[PBO_EMIT_DATA], 0, a.size() * sizeof(Photon), a.data());

			for (int i = 0; i < a.size()/ PHOTON_BLOCK_NUM; i++)
			{
				for (int j = 0; j < PHOTON_BLOCK_NUM; j+=2)
				{
					auto& _a = a.data()[i * PHOTON_BLOCK_NUM + j];
					auto& _b = a.data()[i * PHOTON_BLOCK_NUM + j+1];
					auto ad = _a.getDir();
					auto bd = _b.getDir();
					printf("[i, j]=[%2d, %2d] Pos = %6.2f, %6.2f, %6.2f Dir = %6.2f, %6.2f, %6.2f \n[i, j]=[%2d, %2d] Pos = %6.2f, %6.2f, %6.2f Dir = %6.2f, %6.2f, %6.2f \n", i, j, _a.mPos.x, _a.mPos.y, _a.mPos.z, ad.x, ad.y, ad.z, i, j+1, _b.mPos.x, _b.mPos.y, _b.mPos.z, bd.x, bd.y, bd.z);
				}
				printf("\n");

			}
		}

#pragma endregion


		// PhotonMapping
//		if (0)
		{
			photonMapping.bind();

			std::vector<unsigned> textures = {
				BrickMap0.getHandle(),
				BrickMap1.getHandle(),
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
				triangleBO[TBO_REFERENCE_COUNT],
				triangleBO[TBO_REFERENCE_DATA],
				photonBO[PBO_DATA],
				photonBO[PBO_LL_HEAD],
				photonBO[PBO_LL],
				photonBO[PBO_EMIT_DATA],
				photonBO[PBO_EMIT_BATCH_DATA_COUNT],
				photonBO[PBO_EMIT_BATCH_DATA],
				PhotonNumBuffer,
			};
			glBindBuffersBase(GL_SHADER_STORAGE_BUFFER, 0, buffers.size(), buffers.data());

			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, photonBO[PBO_EMIT_BLOCK_COUNT]);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 1, photonBO[PBO_MARCH_PROCESS_INDEX]);
			glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2, photonBO[PBO_DATA_COUNT]);

			glDispatchCompute(128, 1, 1);
			glFinish();

		}

#pragma region check_photon
//		if (0)
		{
			std::int32_t a = 0;
			std::int32_t b = 0;
			std::int32_t c = 0;
			std::int32_t d = 0;

			glGetNamedBufferSubData(triangleBO[TBO_ATOMIC], 0, 4, &a);
			glGetNamedBufferSubData(photonBO[PBO_DATA_COUNT], 0, 4, &b);
			glGetNamedBufferSubData(photonBO[PBO_EMIT_BLOCK_COUNT], 0, 4, &c);
			glGetNamedBufferSubData(photonBO[PBO_MARCH_PROCESS_INDEX], 0, 4, &d);
			printf("TriangleLL Num = %d, PhotonLL Num = %d, PhotonEmitBlockCount = %d\n", a, b, c);
			printf("Marching Process Index = %d\n", d);
		}

		if (0)
		{
			std::vector<glm::ivec2> a(BRICK_SIZE *BRICK_SIZE * BRICK_SIZE);
			glGetNamedBufferSubData(photonBO[PBO_EMIT_BATCH_DATA_COUNT], 0, sizeof(decltype(a)::value_type)*a.size(), a.data());
			int i = 0;
			for (auto& _a : a)
			{
				if (_a.x!=0)
				{
					auto ii = c1D3D(i, glm::ivec3(BRICK_SIZE));
					printf("[x,y,z = %5d,%5d,%5d]num = %5d, batch=%5d\n", ii.x, ii.y, ii.z, _a.x, _a.y);
				}
				i++;
			}

		}

		if (0)
		{
			std::vector<Photon> a(10000);
			glGetNamedBufferSubData(photonBO[PBO_DATA], 0, sizeof(decltype(a)::value_type)*a.size(), a.data());
			for (auto& p : a)
			{
				glm::vec3 dir = p.getDir();
				printf("pos =[%6.2f,%6.2f,%6.2f], dir =[%6.2f,%6.2f,%6.2f]\n", p.mPos.x, p.mPos.y, p.mPos.z, dir.x, dir.y, dir.z);
			}
		}

		if(0){
			int uboSize;
			glGetActiveUniformBlockiv(photonMappingCS.getHandle(), 1, GL_UNIFORM_BLOCK_DATA_SIZE, &uboSize);
			{
//				std::vector<unsigned> indices(names_.size());
// 				glGetUniformIndices(programHandle, names_.size(), names_.data(), indices.data());
// 
// 				for (unsigned i = 0; i < indices.size(); i++) {
// 					if (indices[i] == GL_INVALID_INDEX) {
// 						ERROR_PRINT("\"%s\" is not ActiveUniform \n", names_[i]);
// 					}
// 				}
//				uboOffset_.resize(names_.size());
				int data;
				unsigned index = 0;
				glGetActiveUniformsiv(photonMappingCS.getHandle(), 1, &index, GL_UNIFORM_OFFSET, &data);
			}

		}
#pragma endregion

		// 画面からの光線
#if 1
//		if(0)
		{
			photonRendering.bind();

			std::vector<unsigned> textures = {
				BrickMap0.getHandle(),
				BrickMap1.getHandle(),
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
				triangleBO[TBO_REFERENCE_DATA],
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
				glViewport(0, 0, 640, 480);
				glDepthMask(GL_FALSE);
				glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
				Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				DrawHelper::Order()->drawTexture(render.getHandle());
			}
		}
#else
//		if(0)
		{
			glViewport(0, 0, 640, 480);
			glEnable(GL_CULL_FACE);
			glDepthMask(GL_TRUE);
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_ALPHA_TEST);
			Renderer::order()->clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			brickRender.bind();

			glBindVertexArray(BrickVAO);

			std::vector<unsigned> textures = {
//				BrickMap1.getHandle(),
				BrickMap0.getHandle(),
			};
			glBindImageTextures(0, textures.size(), textures.data());

			std::vector<unsigned> uniforms = {
				BrickBO,
			};
			glBindBuffersBase(GL_UNIFORM_BUFFER, 0, uniforms.size(), uniforms.data());

			brickRenderVS.setUniform("PV", camera.getProjection()*camera.getView());
			glDrawArraysInstanced(GL_LINES, 0, 36, BRICK_TOTAL + 1);
//			glDrawArraysInstanced(GL_LINES, 0, 36, BRICK_SUB_TOTAL + 1);
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
	{

		glm::vec3 eye = glm::vec3(0., 30., 0.);
		glm::vec3 target = glm::vec3(0., 90., 0.);
		glm::vec3 foward = glm::normalize(target - eye);
		glm::vec3 side = glm::cross(foward, glm::vec3(0., 1., 0.));
		side = dot(side, side) <= 0.00001 ? glm::vec3(-1.f, 0., 0.) : glm::normalize(side);
		glm::vec3 up = glm::normalize(glm::cross(side, foward));

//		glm::lookAt()
		int a = 0;

	}
	// ライト
//	if(0)
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

//	if (0)
	{
		// 中央のボックス
		Mesh mesh = createMesh(0);
		std::for_each(mesh.mVertex.begin(), mesh.mVertex.end(), [](Vertex& p) {p.mPosition += glm::vec3(0.f, 0.5f, 0.f); p.mPosition *= 50.f; p.mPosition -= glm::vec3(0.f, 100.f, 0.f); });

		Model model;
		model.mMesh.push_back(mesh);

		auto& material = model.mMaterial;
		material.resize(1);
		material[0].mDiffuse = glm::vec4(1.0f, 0.f, 0.f, 1.f);

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

	if(0)
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
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);
		material[index++].mDiffuse = glm::vec4(0.8f, 0.8f, 1.0f, 1.f);

		mScene.push(model);

	}

	mScene.setup();


	{
		Triangle tri(glm::vec3(-100.f, -100.f, 0.f), glm::vec3(0.f, 100.f, 0.f), glm::vec3(100.f, -100.f, 0.f));
		tri.scale(2.f);
		Ray ray(glm::vec3(0.f, 0.f, -100.f), glm::vec3(0.f, 0.f, 1.f));
	}

	BrickParam param;
	param.areaMax = glm::vec3(20);
	param.areaMin = glm::vec3(-20);
	param.size0 = glm::ivec3(BRICK_SIZE);
	param.size1 = glm::ivec3(BRICK_SUB_SIZE);

	auto pos = glm::vec3(10, 11, 3);
	auto dir = glm::normalize(glm::vec3(0.f, -4.f, -0.f));
	auto cellSize = (param.areaMax - param.areaMin) / glm::vec3(param.size1);
	glm::ivec3 cell((pos - param.areaMin) / cellSize);

	auto f = [&]()
	{
		using namespace glm;
		glm::vec3 cellOrigin = glm::vec3(cell) * cellSize + param.areaMin;
		glm::vec3 p = (pos) - cellOrigin;
		float x = dir.x < 0. ? p.x : (cellSize.x - p.x);
		float y = dir.y < 0. ? p.y : (cellSize.y - p.y);
		float z = dir.z < 0. ? p.z : (cellSize.z - p.z);
		// 0除算回避
		glm::vec3 dist = glm::vec3(0.);
#if 1
		dist.x = abs(dir.x) < 0.0001 ? 9999.9 : abs(x / dir.x);
		dist.y = abs(dir.y) < 0.0001 ? 9999.9 : abs(y / dir.y);
		dist.z = abs(dir.z) < 0.0001 ? 9999.9 : abs(z / dir.z);
#else
		dist = abs(vec3(x, y, z) / dir);
#endif

		glm::ivec3 next = glm::ivec3(0);
		if (dist.x < dist.y && dist.x < dist.z) {
			next.x = dir.x < 0. ? -1 : 1;
		}
		else if (dist.y < dist.z) {
			next.y = dir.y < 0. ? -1 : 1;
		}
		else {
			next.z = dir.z < 0. ? -1 : 1;
		}
		float rate = min(min(dist.x, dist.y), dist.z);
		pos += dir * rate;
		return next;
	};
// 	for (;;)
// 	{
//  		cell += f();
//  		printf("pos = [%5.2f,%5.2f,%5.2f], cell = [%3d,%3d,%3d]\n", pos.x, pos.y, pos.z, cell.x, cell.y, cell.z);
// 	}

	if(0)
	{
		Texture Map;
		{
			glm::ivec3 brickSize(4);
			Map.create(TextureTarget::TEXTURE_3D);
			std::vector <std::uint8_t> data(brickSize.x*brickSize.y*brickSize.z);
			for (int i = 0; i < 64; i++)
			{
				data[i] = i;
			}
			Map.image3D(0, InternalFormat::R8UI, brickSize.x, brickSize.y, brickSize.z, Format::RED_INTEGER, PixelType::UNSIGNED_BYTE, data.data());
			glGetTextureImage(Map.getHandle(), 0, Format::RED_INTEGER, PixelType::UNSIGNED_BYTE, data.size() * 1, data.data());

			int aa = 0;
		}

	}

	if(0)
	{
		for (;;)
		{
// 			glm::vec3 dir = glm::normalize(glm::ballRand(1.f));
// 			float pi = glm::pi<float>(); 
// 			float intmax = static_cast<float>(INT16_MAX);
// 			std::int16_t theta = acos(dir.z) / pi * intmax;
// 			std::int16_t phi = (dir.y<0.f?-1.f:1.f) * acos(dir.x/sqrt(dir.x*dir.x+ dir.y*dir.y)) / pi * intmax;
// 
// 			glm::vec3 dir2(
// 				sin(theta*pi/intmax)*cos(phi*pi/intmax),
// 				sin(theta*pi/intmax)*sin(phi*pi/intmax),
// 				cos(theta*pi/intmax));
// 
// 			printf("dir =[%6.2f,%6.2f,%6.2f], [theta, phi]=[%d, %d], \ndir2=[%6.2f,%6.2f,%6.2f]\n", dir.x, dir.y, dir.z, theta, phi, dir2.x, dir2.y, dir2.z);

			Photon p;
			glm::vec3 dir = glm::normalize(glm::ballRand(1.f));
			p.setDir(dir);
			glm::vec3 dir2 = p.getDir();
	 		printf("dir =[%6.2f,%6.2f,%6.2f], [theta, phi]=[%d, %d], \ndir2=[%6.2f,%6.2f,%6.2f]\n", dir.x, dir.y, dir.z, 0, 0, dir2.x, dir2.y, dir2.z);
		}
	}
}


}
