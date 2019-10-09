#include "DrawHelper.h"
#include <ogl/ogl.h>
#include <ogldef.h>
#include <Util.h>
#include <Renderer.h>
#include <unordered_map>

static short lineFace[] = {
	0, 1,
};
static float line[] = {
	0.f, 0.f, -1.f,
	0.f, 0.f, 1.f,
};
static short planeFace[] = {
	0, 1, 2,
	1, 3, 2,
};

static float plane[] =
{
/*	// x y	u v
	0.f, 0.f, 0.f, 0.f,
	1.f, 0.f, 1.f, 0.f,
	1.f, 1.f, 1.f, 1.f,
	0.f, 1.f, 0.f, 1.f,
*/	// x y	u v
	-1.f,  -1.f, 0.f, 0.f,
	1.f,  -1.f, 1.f, 0.f,
	-1.f,  1.f, 0.f, 1.f,
	1.f,  1.f, 1.f, 1.f,
};

static float box[] = {
	-0.5f, 0.5f, 0.5f, 0.f,
	0.5f, 0.5f, 0.5f, 0.f,
	-0.5f, -0.5f, 0.5f, 0.f,
	0.5f, -0.5f, 0.5f, 0.f,
	-0.5f, 0.5f, -0.5f, 0.f,
	0.5f, 0.5f, -0.5f, 0.f,
	-0.5f, -0.5f, -0.5f, 0.f,
	0.5f, -0.5f, -0.5f, 0.f,
};

static short boxFace[] =
{
	0, 2, 3,
	0, 3, 1,
	0, 1, 5,
	0, 5, 4,
	6, 7, 3,
	6, 3, 2,
	0, 4, 6,
	0, 6, 2,
	3, 7, 5,
	3, 5, 1,
	5, 7, 6,
	5, 6, 4,
};

float sphere[] = {
	0.271f, -0.901f, -0.339f,
	-0.097f, -0.901f, -0.423f,
	-0.391f, -0.901f, -0.188f,
	-0.391f, -0.901f, 0.188f,
	-0.097f, -0.901f, 0.423f,
	0.271f, -0.901f, 0.339f,
	0.434f, -0.901f, 0.000f,
	0.487f, -0.623f, -0.611f,
	-0.174f, -0.623f, -0.762f,
	-0.704f, -0.623f, -0.339f,
	-0.704f, -0.623f, 0.339f,
	-0.174f, -0.623f, 0.762f,
	0.487f, -0.623f, 0.611f,
	0.782f, -0.623f, 0.000f,
	0.608f, -0.223f, -0.762f,
	-0.217f, -0.223f, -0.950f,
	-0.878f, -0.223f, -0.423f,
	-0.878f, -0.223f, 0.423f,
	-0.217f, -0.223f, 0.950f,
	0.608f, -0.223f, 0.762f,
	0.975f, -0.223f, 0.000f,
	0.608f, 0.223f, -0.762f,
	-0.217f, 0.223f, -0.950f,
	-0.878f, 0.223f, -0.423f,
	-0.878f, 0.223f, 0.423f,
	-0.217f, 0.223f, 0.950f,
	0.608f, 0.223f, 0.762f,
	0.975f, 0.223f, 0.000f,
	0.487f, 0.623f, -0.611f,
	-0.174f, 0.623f, -0.762f,
	-0.704f, 0.623f, -0.339f,
	-0.704f, 0.623f, 0.339f,
	-0.174f, 0.623f, 0.762f,
	0.487f, 0.623f, 0.611f,
	0.782f, 0.623f, 0.000f,
	0.271f, 0.901f, -0.339f,
	-0.097f, 0.901f, -0.423f,
	-0.391f, 0.901f, -0.188f,
	-0.391f, 0.901f, 0.188f,
	-0.097f, 0.901f, 0.423f,
	0.271f, 0.901f, 0.339f,
	0.434f, 0.901f, 0.000f,
	0.000f, -1.000f, 0.000f,
	0.000f, 1.000f, 0.00f,
};
short sphereIndexs[] = {
	0, 1, 7,
	7, 1, 8,
	1, 2, 8,
	8, 2, 9,
	2, 3, 9,
	9, 3, 10,
	3, 4, 10,
	10, 4, 11,
	4, 5, 11,
	11, 5, 12,
	5, 6, 12,
	12, 6, 13,
	6, 0, 13,
	13, 0, 7,
	7, 8, 14,
	14, 8, 15,
	8, 9, 15,
	15, 9, 16,
	9, 10, 16,
	16, 10, 17,
	10, 11, 17,
	17, 11, 18,
	11, 12, 18,
	18, 12, 19,
	12, 13, 19,
	19, 13, 20,
	13, 7, 20,
	20, 7, 14,
	14, 15, 21,
	21, 15, 22,
	15, 16, 22,
	22, 16, 23,
	16, 17, 23,
	23, 17, 24,
	17, 18, 24,
	24, 18, 25,
	18, 19, 25,
	25, 19, 26,
	19, 20, 26,
	26, 20, 27,
	20, 14, 27,
	27, 14, 21,
	21, 22, 28,
	28, 22, 29,
	22, 23, 29,
	29, 23, 30,
	23, 24, 30,
	30, 24, 31,
	24, 25, 31,
	31, 25, 32,
	25, 26, 32,
	32, 26, 33,
	26, 27, 33,
	33, 27, 34,
	27, 21, 34,
	34, 21, 28,
	28, 29, 35,
	35, 29, 36,
	29, 30, 36,
	36, 30, 37,
	30, 31, 37,
	37, 31, 38,
	31, 32, 38,
	38, 32, 39,
	32, 33, 39,
	39, 33, 40,
	33, 34, 40,
	40, 34, 41,
	34, 28, 41,
	41, 28, 35,
	1, 0, 42,
	2, 1, 42,
	3, 2, 42,
	4, 3, 42,
	5, 4, 42,
	6, 5, 42,
	0, 6, 42,
	35, 36, 43,
	36, 37, 43,
	37, 38, 43,
	38, 39, 43,
	39, 40, 43,
	40, 41, 43,
	41, 35, 43
};
DrawHelper* DrawHelper::instance_;

DrawHelper* DrawHelper::Order()
{
	return instance_;
}

void DrawHelper::Initialize()
{
	instance_ = new DrawHelper;
}
void DrawHelper::Finalize()
{
	delete instance_;
	instance_ = NULL;
}

DrawHelper::DrawHelper(void)
{
	program_.compileShaderFromFile("../Resource/Shader/simpl2d.vs", Program::SHADER_VERTEX);
	program_.compileShaderFromFile("../Resource/Shader/simpl2d.fs", Program::SHADER_FRAGMENT);
	program_.link();

	programTex2DArray_.compileShaderFromFile("../Resource/Shader/Helper/simpl2dtexarray.vs", Program::SHADER_VERTEX);
	programTex2DArray_.compileShaderFromFile("../Resource/Shader/Helper/simpl2dtexarray.fs", Program::SHADER_FRAGMENT);
	programTex2DArray_.link();

	
	textureBufferProgram_.compileShaderFromFile("../Resource/Shader/simpl2dtexbuffer.vs", Program::SHADER_VERTEX);
	textureBufferProgram_.compileShaderFromFile("../Resource/Shader/simpl2dtexbuffer.fs", Program::SHADER_FRAGMENT);
	textureBufferProgram_.link();
//	textureBufferProgram_.createUniformBlock("Data", { "center", "size", "z" });

	glGenVertexArrays(1, &vao2DHandle_);
	glBindVertexArray(vao2DHandle_);

	glGenBuffers(1, &faceHanfle_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceHanfle_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(planeFace), planeFace, GL_STATIC_DRAW);

	unsigned buffer;
	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(plane), plane, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, 0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	programBox_.compileShaderFromFile("../Resource/Shader/simpl3d.vs", Program::SHADER_VERTEX);
	programBox_.compileShaderFromFile("../Resource/Shader/simpl3d.fs", Program::SHADER_FRAGMENT);
	programBox_.link();

	programTexture_.compileShaderFromFile("../Resource/Shader/simpl2dtex.vs", Program::SHADER_VERTEX);
	programTexture_.compileShaderFromFile("../Resource/Shader/simpl2dtex.fs", Program::SHADER_FRAGMENT);
	programTexture_.link();

	glGenVertexArrays(1, &vaoBoxHandle_);
	glBindVertexArray(vaoBoxHandle_);

	glGenBuffers(1, &faceBoxHandle_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceBoxHandle_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(boxFace), boxFace, GL_STATIC_DRAW);

	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(box), box, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 16, 0);
	glEnableVertexAttribArray(0);

	glGenVertexArrays(1, &vaoSphereHandle_);
	glBindVertexArray(vaoSphereHandle_);

	glGenBuffers(1, &faceSphereHandle_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceSphereHandle_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(sphereIndexs), sphereIndexs, GL_STATIC_DRAW);

	glGenBuffers(1, &buffer);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(sphere), sphere, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
	glEnableVertexAttribArray(0);

	glGenVertexArrays(1, &vaoLineHandle_);
	glBindVertexArray(vaoLineHandle_);

	glGenBuffers(1, &vboLineHandle_);
	glBindBuffer(GL_ARRAY_BUFFER, vboLineHandle_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(line), line, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 12, 0);
	glEnableVertexAttribArray(0);

	glGenBuffers(1, &faceLineHandle_);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceLineHandle_);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(lineFace), lineFace, GL_STATIC_DRAW);

	glBindVertexArray(0);

	font_.create(L"MSÉSÉVÉbÉN", 8);
}


DrawHelper::~DrawHelper(void)
{
}


template<typename T>
struct Value
{
	Value(const T& t) : value(t){}
	T value;
};
void DrawHelper::draw2DPoint(float x, float y, float z)
{
	program_.use();
	program_.setUniform("center", vec2(x, y));
	program_.setUniform("size", 5.f);
	program_.setUniform("z", z);
	glBindVertexArray(vao2DHandle_);
	glDrawElements(GL_POINTS, 1, GL_UNSIGNED_SHORT, NULL);

}
void DrawHelper::draw2DPoint()
{
	glBindVertexArray(vao2DHandle_);
	glDrawElements(GL_POINTS, 1, GL_UNSIGNED_SHORT, NULL);
	glBindVertexArray(0);

}

void DrawHelper::draw2DRect()
{
	glBindVertexArray(vao2DHandle_);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
	glBindVertexArray(0);
}

void DrawHelper::draw2DRect(float x, float y, float width, float height, float z)
{
	program_.use();
	program_.setUniform("center", vec2(x + width / 2.f, (1.f - y) + height / 2.f));
	program_.setUniform("size", vec2(width, height));
	program_.setUniform("z", z);
	glBindVertexArray(vao2DHandle_);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);
}

void DrawHelper::draw2DLine(float x0, float y0, float x1, float y1, float width, float z)
{
	glUseProgram(0);
	glBindProgramPipeline(0);
	::glLineWidth(50);                                      //      ê¸ÇÃëæÇ≥
	::glBegin(GL_LINES);                                    //      ê¸ï™ÇÃï`âÊ
	::glColor3f(1.0, 0.0, 1.0);
	::glVertex2f(x0, y0);
	::glColor3f(0.0, 1.0, 1.0);
	::glVertex2f(x1, y1);
	::glEnd();
}

void DrawHelper::drawPlane(const glm::vec3& tl, const glm::vec3& tr, const glm::vec3& bl, const glm::vec3& br)
{
	::glBegin(GL_QUADS);                                    //      ê¸ï™ÇÃï`âÊ
	::glVertex3fv(glm::value_ptr(tl));
	::glVertex3f(tr.x, tr.y, tr.z);
	::glVertex3f(bl.x, bl.y, bl.z);
	::glVertex3f(br.x, br.y, br.z);
	::glEnd();
	/*	unsigned buffer;
	glCreateBuffers(1, &buffer);
	std::vector<glm::vec3> data = { tl, tr, br, bl };
	glNamedBufferStorage(buffer, 3 * 4 * 4, data.data(), GL_MAP_WRITE_BIT | GL_MAP_READ_BIT);
	glBindBuffer(GL_ARRAY_BUFFER, buffer);
	glDrawArrays(GL_QUAD, 0, 4);
*/
}

void DrawHelper::draw3DLine(float x0, float y0, float z0, float x1, float y1, float z1, float width, const DrawParam& param)
{
	programBox_.use();
	
	programBox_.setUniform("MVP", Renderer::order()->camera().projection() * Renderer::order()->camera().view());
	programBox_.setUniform("World", glm::mat4(1.f));
	programBox_.setUniform("Color", param.color);

	glBindVertexArray(vaoLineHandle_);
	glBindBuffer(GL_ARRAY_BUFFER, vboLineHandle_);
	float* buffer = reinterpret_cast<float*>(glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY));
	buffer[0] = x0;
	buffer[1] = y0;
	buffer[2] = z0;
	buffer[3] = x1;
	buffer[4] = y1;
	buffer[5] = z1;
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceLineHandle_);
	::glLineWidth(width);                                      //      ê¸ÇÃëæÇ≥
	glDrawElements(GL_LINES, 2, GL_UNSIGNED_SHORT, NULL);

}

void DrawHelper::draw3DLine(const glm::vec3& pos0, const glm::vec3& pos1, float width, const DrawParam& param)
{
	draw3DLine(pos0.x, pos0.y, pos0.z, pos1.x, pos1.y, pos1.z, width, param);
}

void DrawHelper::draw2DRect(float x, float y, float width, float height, float z, int color)
{
}

void DrawHelper::draw3DBox(float x, float y, float z, float width, float height, float depth, const quat& rotate)
{
	programBox_.use();
	UniformBuffer* uniform = programBox_.getUniformBuffer("Data");
	uniform->setMemory("MVP", glm::value_ptr(Renderer::order()->camera().projection() * Renderer::order()->camera().view()), 64);
	mat4 world;
	world = glm::translate(vec3(x, y, z));
	world *= glm::toMat4(rotate);
	world = glm::scale(world, vec3(width, height, depth));
	uniform->setMemory("World", glm::value_ptr(world), 64);

	glBindVertexArray(vaoBoxHandle_);
//	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceBoxHandle_);
	glDrawElements(GL_LINES, 36, GL_UNSIGNED_SHORT, NULL);

}

void DrawHelper::draw3DSphere(float x, float y, float z, float r, const quat& rotate /*= glm::quat()*/, const glm::vec4& color /*= glm::vec4(1.f)*/)
{
	programBox_.use();
	mat4 world;
	world = glm::translate(vec3(x, y, z));
	world *= glm::toMat4(rotate);
	world = glm::scale(world, vec3(r, r, r));

	programBox_.setUniform("MVP", Renderer::order()->camera().projection() * Renderer::order()->camera().view());
	programBox_.setUniform("World", world);
	programBox_.setUniform("Color", color);

	glBindVertexArray(vaoSphereHandle_);
	glDrawElements(GL_TRIANGLES, 252, GL_UNSIGNED_SHORT, NULL);

}

void DrawHelper::draw3DSphere(const glm::vec3& pos, float r, const DrawParam& param /*= DrawParam()*/)
{
	draw3DSphere(pos.x, pos.y, pos.z, r, param.rot, param.color);
}

void DrawHelper::drawVertex(const std::vector<glm::vec4>& vertex)
{
//	programBox_.setUniform("MVP", Renderer::order()->camera().projection() * Renderer::order()->camera().view());
//	programBox_.setUniform("World", world);
//	programBox_.setUniform("Color", color);
}

void DrawHelper::drawTexture(unsigned tex, float scale /*= 1.f*/)
{
	program_.use();
	program_.setUniform("center", vec2(0, 0));
	program_.setUniform("size", vec2(1, 1));
	program_.setUniform("z", 0.f);

	glBindTextureUnit(0, tex);
	glBindVertexArray(vao2DHandle_);

	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

}

void DrawHelper::drawTextureArray(unsigned texHandle)
{
	programTex2DArray_.use();

	glBindTextureUnit(0, texHandle);

	glBindVertexArray(vao2DHandle_);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

}

void DrawHelper::drawShadow(unsigned tex, float scale /*= 1.f*/)
{
	program_.use();
	program_.setUniform("center", vec2(0, 0));
	program_.setUniform("size", vec2(1, 1));
	program_.setUniform("z", 0.f);

	glActiveTexture(GL_TEXTURE31);
	glBindTexture(GL_TEXTURE_2D, tex);
	//	int loc = glGetUniformLocation(program_.getHandle(), "uTex0");
	int loc = glGetUniformLocation(program_.getHandle(), "uShadowMap");
	glUniform1i(loc, 31);
	glBindVertexArray(vao2DHandle_);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, faceHanfle_);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, NULL);

}

void DrawHelper::printf(int x, int y, const wchar_t *format, ...)
{
	wchar_t buf[256];
	va_list ap;
	va_start(ap, format);
	vswprintf_s(buf, format, ap);
	font_.drawStringImpl(x, y, buf);
	va_end(ap);

}


void DrawHelper::GetPlane(std::vector<glm::vec3>& vertex, std::vector<unsigned>& element)
{
	static glm::vec3 _plane[] =
	{
		{-0.5f,  0.f,  -0.5f},
		{ -0.5f,  0.f,   0.5f },
		{ 0.5f,  0.f,  -0.5f},
		{ 0.5f,  0.f,   0.5f},
	};
	vertex.resize(sizeof(_plane) / sizeof(_plane[0]));
	for (size_t i = 0; i < vertex.size(); i++){
		vertex[i] = _plane[i];
	}
	element.resize(sizeof(planeFace) / sizeof(planeFace[0]));
	{
		for (size_t i = 0; i < sizeof(planeFace) / sizeof(planeFace[0]); i++){
			element[i] = planeFace[i];
		}

	}

}

void DrawHelper::GetBox(std::vector<glm::vec3>& vertex)
{
	glm::vec3 b[] = 
	{
		// x
		glm::vec3(0.5f, 0.5f, 0.5f),
		glm::vec3(0.5f, -0.5f, -0.5f),
		glm::vec3(0.5f, 0.5f, -0.5f),
		glm::vec3(0.5f, 0.5f, 0.5f),
		glm::vec3(0.5f, -0.5f, 0.5f),
		glm::vec3(0.5f, -0.5f, -0.5f),

		// -x
		glm::vec3(-0.5f, 0.5f, 0.5f),
		glm::vec3(-0.5f, 0.5f, -0.5f),
		glm::vec3(-0.5f, -0.5f, -0.5f),
		glm::vec3(-0.5f, 0.5f, 0.5f),
		glm::vec3(-0.5f, -0.5f, -0.5f),
		glm::vec3(-0.5f, -0.5f, 0.5f),

		// y
		glm::vec3(0.5f, 0.5f, 0.5f),
		glm::vec3(0.5f, 0.5f, -0.5f),
		glm::vec3(-0.5f, 0.5f, -0.5f),
		glm::vec3(0.5f, 0.5f, 0.5f),
		glm::vec3(-0.5f, 0.5f, -0.5f),
		glm::vec3(-0.5f, 0.5f, 0.5f),

		// -y
		glm::vec3(0.5f, -0.5f, 0.5f),
		glm::vec3(-0.5f, -0.5f, -0.5f),
		glm::vec3(0.5f, -0.5f, -0.5f),
		glm::vec3(0.5f, -0.5f, 0.5f),
		glm::vec3(-0.5f, -0.5f, 0.5f),
		glm::vec3(-0.5f, -0.5f, -0.5f),
		// z
		glm::vec3(0.5f, 0.5f, 0.5f),
		glm::vec3(-0.5f, 0.5f, 0.5f),
		glm::vec3(-0.5f, -0.5f, 0.5f),
		glm::vec3(0.5f, 0.5f, 0.5f),
		glm::vec3(-0.5f, -0.5f, 0.5f),
		glm::vec3(0.5f, -0.5f, 0.5f),

		// -z
		glm::vec3(-0.5f, -0.5f, -0.5f),
		glm::vec3(-0.5f, 0.5f, -0.5f),
		glm::vec3(0.5f, 0.5f, -0.5f),
		glm::vec3(-0.5f, -0.5f, -0.5f),
		glm::vec3(0.5f, 0.5f, -0.5f),
		glm::vec3(0.5f, -0.5f, -0.5f),
	};

	vertex.clear();
	vertex.resize(sizeof(b) / sizeof(b[0]));
	for (size_t i = 0; i < vertex.size(); i++)
	{
		vertex[i] = b[i];
	}
}

void DrawHelper::GetBox(std::vector<float>& vertex, std::vector<unsigned>& element)
{

	vertex.resize(sizeof(box) / sizeof(box[0]));
	for (size_t i = 0; i < sizeof(box) / sizeof(box[0]); i++){
		vertex[i] = box[i];
	}
	element.resize(sizeof(boxFace) / sizeof(boxFace[0]));
	{
		for (size_t i = 0; i < sizeof(boxFace) / sizeof(boxFace[0]); i++){
			element[i] = boxFace[i];
		}

	}
}
void DrawHelper::GetBox(std::vector<glm::vec3>& vertex, std::vector<unsigned>& element)
{
	vertex.resize(sizeof(box) / sizeof(box[0]) / 4);
	for (size_t i = 0; i < sizeof(box) / sizeof(box[0]) / 4; i++){
		vertex[i] = vec3(box[i * 4], box[i * 4 + 1], box[i * 4 + 2]);
	}
	element.resize(sizeof(boxFace) / sizeof(boxFace[0]));
	{
		for (size_t i = 0; i < sizeof(boxFace) / sizeof(boxFace[0]); i++){
			element[i] = boxFace[i];
		}

	}
}

std::tuple<std::vector<glm::vec3>, std::vector<glm::ivec3>> DrawHelper::GetPlane()
{
	std::vector<glm::vec3> v(4);
	v[0] = { -0.5f, 0.f, -0.5f };
	v[1] = { -0.5f, 0.f, 0.5f };
	v[2] = { 0.5f, 0.f, -0.5f };
	v[3] = { 0.5f, 0.f, 0.5f };

	std::vector<glm::ivec3> i(2);
	i[0] = { 0, 1, 2 };
	i[1] = { 1, 3, 2 };

	return std::tie(v, i);
}

std::tuple<std::vector<glm::vec3>, std::vector<glm::ivec3>> DrawHelper::GetBox()
{
	std::tuple<std::vector<glm::vec3>, std::vector<glm::ivec3>> ret;
	auto& vertex = std::get<0>(ret);
	auto& element = std::get<1>(ret);
	vertex.resize(sizeof(box) / sizeof(box[0]) / 4);
	for (size_t i = 0; i < sizeof(box) / sizeof(box[0]) / 4; i++) {
		vertex[i] = vec3(box[i * 4], box[i * 4 + 1], box[i * 4 + 2]);
	}
	element.reserve(sizeof(boxFace) / sizeof(boxFace[0]) / 3);
	{
		for (size_t i = 0; i < sizeof(boxFace) / sizeof(boxFace[0]); i+=3) {
			element.emplace_back(boxFace[i], boxFace[i+1], boxFace[i+2]);
		}

	}
	return ret;

}

// http://blog.andreaskahler.com/2009/06/creating-icosphere-mesh-in-code.html
class SphereCreator
{

	std::vector<glm::vec3> vertex;
	std::unordered_map<std::int64_t, int> middlePointIndexCache;

	// add vertex to mesh, fix position to be on unit sphere, return index
	int addVertex(const glm::vec3& p)
	{
		vertex.emplace_back(glm::normalize(p));
		return vertex.size()-1;
	}

	// return index of point in the middle of p1 and p2
	int getMiddlePoint(int p1, int p2)
	{
		// first check if we have it already
		bool firstIsSmaller = p1 < p2;
		std::int64_t smallerIndex = firstIsSmaller ? p1 : p2;
		std::int64_t greaterIndex = firstIsSmaller ? p2 : p1;
		std::int64_t key = (smallerIndex << 32) + greaterIndex;

		auto it = middlePointIndexCache.find(key);
		if (it != middlePointIndexCache.end())
		{
			return it->second;
		}

		// not in cache, calculate it
		glm::vec3 point1 = vertex[p1];
		glm::vec3 point2 = vertex[p2];
		glm::vec3 middle = (point1 + point2) / 2.f;

		// add vertex makes sure point is on unit sphere
		int i = addVertex(middle);

		// store it, return index
		middlePointIndexCache[key] = i;
		return i;
	}

public:
	std::tuple<std::vector<glm::vec3>, std::vector<glm::ivec3>> create(int recursionLevel)
	{
		// create 12 vertices of a icosahedron
		auto t = (1.f + glm::sqrt(5.f)) / 2.f;

		addVertex(glm::vec3(-1,  t, 0));
		addVertex(glm::vec3( 1,  t, 0));
		addVertex(glm::vec3(-1, -t, 0));
		addVertex(glm::vec3( 1, -t, 0));

		addVertex(glm::vec3(0, -1,  t));
		addVertex(glm::vec3(0,  1,  t));
		addVertex(glm::vec3(0, -1, -t));
		addVertex(glm::vec3(0,  1, -t));

		addVertex(glm::vec3( t,  0, -1));
		addVertex(glm::vec3( t,  0,  1));
		addVertex(glm::vec3(-t,  0, -1));
		addVertex(glm::vec3(-t,  0,  1));


		// create 20 triangles of the icosahedron
		std::vector<glm::ivec3> face;

		// 5 faces around point 0
		face.emplace_back( 0, 11,  5);
		face.emplace_back( 0,  5,  1);
		face.emplace_back( 0,  1,  7);
		face.emplace_back( 0,  7, 10);
		face.emplace_back( 0, 10, 11);

		// 5 adjacent faces 
		face.emplace_back( 1,  5,  9);
		face.emplace_back( 5, 11,  4);
		face.emplace_back(11, 10,  2);
		face.emplace_back(10,  7,  6);
		face.emplace_back( 7,  1,  8);

		// 5 faces around point 3
		face.emplace_back( 3,  9,  4);
		face.emplace_back( 3,  4,  2);
		face.emplace_back( 3,  2,  6);
		face.emplace_back( 3,  6,  8);
		face.emplace_back( 3,  8,  9);

		// 5 adjacent faces 
		face.emplace_back( 4,  9,  5);
		face.emplace_back( 2,  4, 11);
		face.emplace_back( 6,  2, 10);
		face.emplace_back( 8,  6,  7);
		face.emplace_back( 9,  8,  1);


		// refine triangles
		for (int i = 0; i < recursionLevel; i++)
		{
//			vertex.clear();
//			middlePointIndexCache.clear();
//			index = 0;
			std::vector<glm::ivec3> face2;
			for(auto& f : face)
			{
				// replace triangle by 4 triangles
				int a = getMiddlePoint(f.x, f.y);
				int b = getMiddlePoint(f.y, f.z);
				int c = getMiddlePoint(f.z, f.x);

				face2.emplace_back(f.x, a, c);
				face2.emplace_back(f.y, b, a);
				face2.emplace_back(f.z, c, b);
				face2.emplace_back(a, b, c);
			}
			face = face2;
		}

		return std::make_tuple(vertex, face);
	}
};
void DrawHelper::GetSphere(std::vector<glm::vec3>& vertex, std::vector<unsigned>& element, int quarity /*= 0*/)
{
	vertex.clear();
	element.clear();
	auto t = SphereCreator().create(quarity);
	vertex = std::get<0>(t);
	auto& e = std::get<1>(t);

	element.reserve(e.size() * 3);
	for (auto& _e : e)
	{
		for (size_t i = 0; i < _e.length(); i++)
		{
			element.emplace_back(_e[i]);
		}
	}
	vertex.shrink_to_fit();
	element.shrink_to_fit();
}

std::tuple < std::vector<glm::vec3>, std::vector<glm::ivec3> > DrawHelper::GetSphere(int quarity /*= 0*/)
{
	return SphereCreator().create(quarity);
}

std::vector<glm::vec3> DrawHelper::CalcNormal(const std::vector<glm::vec3>& vertex)
{
	// "drawArrayÇ≈égÇ§Ç‚Ç¬óp"
	assert(vertex.size()%3 == 0);
	std::vector<glm::vec3> normal(vertex.size());
	for (size_t i = 0; i < vertex.size(); i += 3)
	{
		glm::vec3 a = vertex[i].xyz;
		glm::vec3 b = vertex[i + 1].xyz;
		glm::vec3 c = vertex[i + 2].xyz;
		glm::vec3 ab = glm::normalize(b - a);
		glm::vec3 ac = glm::normalize(c - a);
		glm::vec3 cross = glm::cross(ab, ac);
		normal[i] += cross;
		normal[i + 1] += cross;
		normal[i + 2] += cross;
	}

	for (auto& _n : normal)
	{
		_n = glm::normalize(_n);
	}
	return normal;

}

std::vector<glm::vec3> DrawHelper::CalcNormal(const std::vector<glm::vec3>& vertex, const std::vector<unsigned>& element)
{
	std::vector<glm::vec3> normal(vertex.size());
	for (size_t i = 0; i < element.size(); i += 3)
	{
		glm::vec3 a = vertex[element[i]].xyz;
		glm::vec3 b = vertex[element[i + 1]].xyz;
		glm::vec3 c = vertex[element[i + 2]].xyz;
		glm::vec3 ab = glm::normalize(b - a);
		glm::vec3 ac = glm::normalize(c - a);
		glm::vec3 cross = glm::cross(ab, ac);
		normal[element[i]] += cross;
		normal[element[i + 1]] += cross;
		normal[element[i + 2]] += cross;
	}

	for (auto& _n : normal)
	{
		_n = glm::normalize(_n);
	}
	return normal;

}

std::vector<glm::vec3> DrawHelper::CalcNormal(const std::vector<glm::vec3>& vertex, const std::vector<glm::ivec3>& element)
{
	std::vector<glm::vec3> normal(vertex.size());
	for (size_t i = 0; i < element.size(); i ++)
	{
		glm::vec3 a = vertex[element[i].x].xyz;
		glm::vec3 b = vertex[element[i].y].xyz;
		glm::vec3 c = vertex[element[i].z].xyz;
		glm::vec3 ab = glm::normalize(b - a);
		glm::vec3 ac = glm::normalize(c - a);
		glm::vec3 cross = glm::cross(ab, ac);
		normal[element[i].x] += cross;
		normal[element[i].y] += cross;
		normal[element[i].z] += cross;
	}

	for (auto& _n : normal)
	{
		_n = glm::normalize(_n);
	}
	return normal;

}


