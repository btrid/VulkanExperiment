#include <btrlib/rTexture.h>
#include <btrlib/Define.h>
#include <memory>
#include <functional>

#define USE_FREEIMAGE
//#define USE_DEVIL

#ifdef USE_FREEIMAGE

#include <FreeImage.h>

struct Init
{
	Init()
	{
		FreeImage_Initialise(true);
		FreeImage_SetOutputMessage((FreeImage_OutputMessageFunction)[](FREE_IMAGE_FORMAT fif, const char *msg) { printf(msg); });
	}

	~Init() 
	{ 
		FreeImage_DeInitialise(); 
	}
};
rTexture::Data rTexture::LoadTexture(const std::string& file, const LoadParam& param /*= LoadParam()*/)
{
	static Init init;

	FREE_IMAGE_FORMAT format = FreeImage_GetFileType(file.c_str());

	// Found image, but couldn't determine the file format? Try again...
	if (format == FIF_UNKNOWN)
	{
		printf("Couldn't determine file format - attempting to get from file extension...\n");
		format = FreeImage_GetFIFFromFilename(file.c_str());

		// Check that the plugin has reading capabilities for this format (if it's FIF_UNKNOWN,
		// for example, then it won't have) - if we can't read the file, then we bail out =(
		if (!FreeImage_FIFSupportsReading(format))
		{
			printf("Detected image format cannot be read!\n");
			assert(false);
			return rTexture::Data();
		}
	}

	auto bitmap = std::shared_ptr<FIBITMAP>(FreeImage_Load(format, file.c_str()), FreeImage_Unload);
	auto bitmapOld = bitmap;
	bitmap = std::shared_ptr<FIBITMAP>(FreeImage_ConvertTo32Bits(bitmapOld.get()), FreeImage_Unload);

	// Some basic image info - strip it out if you don't care
	int bpp = FreeImage_GetBPP(bitmap.get());
	int w = FreeImage_GetWidth(bitmap.get());
	int h = FreeImage_GetHeight(bitmap.get());

	rTexture::Data data;
	data.m_data.resize(w*h);
	data.m_size = glm::ivec3(w, h, 1);

	auto* p = (unsigned*)FreeImage_GetBits(bitmap.get());
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++) {
			int index = y*w + x;
			data.m_data[index].r = ((p[index] & FI_RGBA_RED_MASK) >> FI_RGBA_RED_SHIFT) / 255.f;
			data.m_data[index].g = ((p[index] & FI_RGBA_GREEN_MASK) >> FI_RGBA_GREEN_SHIFT) / 255.f;
			data.m_data[index].b = ((p[index] & FI_RGBA_BLUE_MASK) >> FI_RGBA_BLUE_SHIFT) / 255.f;
			data.m_data[index].a = ((p[index] & FI_RGBA_ALPHA_MASK) >> FI_RGBA_ALPHA_SHIFT) / 255.f;
//			data.mData[index] = glm::vec4(p[index]) / 255.f;
		}
	}
	return data;
}
#elif defined(USE_DEVIL)
#include <IL/il.h>
#pragma comment(lib, "DevIL.lib")
static int ConvertDevILFormatToOpenGL(int Format)
{
	switch (Format)
	{
	case IL_COLOR_INDEX:
		Format = GL_COLOR_INDEX;
		break;
	case IL_RGB:
		Format = GL_RGB;
		break;
	case IL_RGBA:
		Format = GL_RGBA;
		break;
	case IL_BGR:
		Format = GL_BGR;
		break;
	case IL_BGRA:
		Format = GL_BGRA;
		break;
	case IL_LUMINANCE:
		Format = GL_LUMINANCE;
		break;
	case IL_LUMINANCE_ALPHA:
		Format = GL_LUMINANCE_ALPHA;
		break;
	default:
		break;
		//                    throw new Exception("Unknown image format");
	}
	return Format;
}

rTexture::Data rTexture::LoadTextureEx(const std::string& file, const LoadParam& param /*= LoadParam()*/)
{
	static bool isInit;
	if (!isInit)
	{
		isInit = true;
		ilInit();
	}

	auto deleter = [](ILuint* id) { if ((*id) != 0) { ilDeleteImages(1, id); delete id; } };
	std::unique_ptr<ILuint, std::function<void(ILuint*)>> id(new ILuint(0), deleter);
	ilGenImages(1, id.get());
	ilBindImage(*id.get());

	if (!ilLoadImage(file.c_str()))
	{

		printf("[ERROR] %s Couldn't load image\n\n", file.c_str());
		return rTexture::Data();
	}
	if (!ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE))
	{
		printf("[ERROR] %s Couldn't convert image\n\n", file.c_str());
		return rTexture::Data();
	}

	
	rTexture::Data data;
	data.m_size = glm::ivec3(ilGetInteger(IL_IMAGE_WIDTH), ilGetInteger(IL_IMAGE_HEIGHT), 1);
	data.m_data.resize(data.m_size.x*data.m_size.y);
	std::memcpy(data.m_data.data(), ilGetData(), data.m_size.x*data.m_size.y * sizeof(glm::vec4));
	return data;
}
#else

rTexture::Data rTexture::LoadTextureEx(const std::string& file, const LoadParam& param /*= LoadParam()*/)
{
	rTexture::Data data;
	data.m_data.push_back(glm::vec4{ 1.f, 0.5f, 0.5f, 1.f });
	data.m_size = glm::ivec3(1, 1, 1);
	return data;
}

#endif
