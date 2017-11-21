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

ResourceManager<ResourceTexture::Resource> ResourceTexture::s_manager;
void ResourceTexture::load(std::shared_ptr<btr::Context>& context, vk::CommandBuffer cmd, const std::string& filename)
{
	if (s_manager.manage(m_resource, filename)) {
		return;
	}

	auto texture_data = rTexture::LoadTexture(filename);
	vk::ImageCreateInfo image_info;
	image_info.imageType = vk::ImageType::e2D;
	image_info.format = vk::Format::eR32G32B32A32Sfloat;
	image_info.mipLevels = 1;
	image_info.arrayLayers = 1;
	image_info.samples = vk::SampleCountFlagBits::e1;
	image_info.tiling = vk::ImageTiling::eLinear;
	image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
	image_info.sharingMode = vk::SharingMode::eExclusive;
	image_info.initialLayout = vk::ImageLayout::eUndefined;
	image_info.extent = { texture_data.m_size.x, texture_data.m_size.y, 1 };
	vk::UniqueImage image = context->m_device->createImageUnique(image_info);

	vk::MemoryRequirements memory_request = context->m_device->getImageMemoryRequirements(image.get());
	vk::MemoryAllocateInfo memory_alloc_info;
	memory_alloc_info.allocationSize = memory_request.size;
	memory_alloc_info.memoryTypeIndex = cGPU::Helper::getMemoryTypeIndex(context->m_device.getGPU(), memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

	vk::UniqueDeviceMemory image_memory = context->m_device->allocateMemoryUnique(memory_alloc_info);
	context->m_device->bindImageMemory(image.get(), image_memory.get(), 0);

	btr::BufferMemoryDescriptor staging_desc;
	staging_desc.size = texture_data.getBufferSize();
	staging_desc.attribute = btr::BufferMemoryAttributeFlagBits::SHORT_LIVE_BIT;
	auto staging_buffer = context->m_staging_memory.allocateMemory(staging_desc);
	memcpy(staging_buffer.getMappedPtr(), texture_data.m_data.data(), texture_data.getBufferSize());

	vk::ImageSubresourceRange subresourceRange;
	subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.layerCount = 1;
	subresourceRange.levelCount = 1;

	{
		// staging_buffer‚©‚çimage‚ÖƒRƒs[

		vk::BufferImageCopy copy;
		copy.bufferOffset = staging_buffer.getBufferInfo().offset;
		copy.imageExtent = { texture_data.m_size.x, texture_data.m_size.y, texture_data.m_size.z };
		copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageSubresource.mipLevel = 0;

		{
			vk::ImageMemoryBarrier to_copy_barrier;
			to_copy_barrier.dstQueueFamilyIndex = context->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
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
			to_shader_read_barrier.dstQueueFamilyIndex = context->m_device.getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);
			to_shader_read_barrier.image = image.get();
			to_shader_read_barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			to_shader_read_barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
			to_shader_read_barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			to_shader_read_barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
			to_shader_read_barrier.subresourceRange = subresourceRange;
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, vk::DependencyFlags(), {}, {}, { to_shader_read_barrier });
		}

	}

	vk::ImageViewCreateInfo view_info;
	view_info.viewType = vk::ImageViewType::e2D;
	view_info.components.r = vk::ComponentSwizzle::eR;
	view_info.components.g = vk::ComponentSwizzle::eG;
	view_info.components.b = vk::ComponentSwizzle::eB;
	view_info.components.a = vk::ComponentSwizzle::eA;
	view_info.flags = vk::ImageViewCreateFlags();
	view_info.format = vk::Format::eR32G32B32A32Sfloat;
	view_info.image = image.get();
	view_info.subresourceRange = subresourceRange;

	vk::SamplerCreateInfo sampler_info;
	sampler_info.magFilter = vk::Filter::eLinear;
	sampler_info.minFilter = vk::Filter::eLinear;
	sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
	sampler_info.addressModeU = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.addressModeV = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.addressModeW = vk::SamplerAddressMode::eClampToEdge;
	sampler_info.mipLodBias = 0.0f;
	sampler_info.compareOp = vk::CompareOp::eNever;
	sampler_info.minLod = 0.0f;
	sampler_info.maxLod = 0.f;
	sampler_info.maxAnisotropy = 1.0;
	sampler_info.anisotropyEnable = VK_FALSE;
	sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

	m_resource->m_image = std::move(image);
	m_resource->m_memory = std::move(image_memory);
	m_resource->m_image_view = context->m_device->createImageViewUnique(view_info);
	m_resource->m_sampler = context->m_device->createSamplerUnique(sampler_info);
}
