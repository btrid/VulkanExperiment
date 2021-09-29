#include <btrlib/Define.h>

#include <btrlib/Context.h>

#include <btrlib/cWindow.h>
#include <btrlib/cInput.h>
#include <btrlib/cCamera.h>
#include <btrlib/sGlobal.h>
#include <btrlib/GPU.h>
#include <btrlib/cStopWatch.h>
#include <btrlib/AllocatedMemory.h>
#include <btrlib/cModel.h>

#include <applib/App.h>
#include <applib/AppPipeline.h>
#include <applib/sCameraManager.h>
#include <applib/sAppImGui.h>
#include <applib/GraphicsResource.h>

#include <gli/gli/gli.hpp>

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#include <imgui/imgui.h>

#pragma comment(lib, "btrlib.lib")
#pragma comment(lib, "applib.lib")
#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "imgui.lib")

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#define JSON_NOEXCEPTION
//#define TINYGLTF_NO_STB_IMAGE
//#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_MSC_SECURE_CRT
#include <tinygltf/tiny_gltf.h>

namespace GLtoVK
{
	vk::Format toFormat(int gltf_type, int component_type)
	{
		switch (gltf_type)
		{
		case TINYGLTF_TYPE_SCALAR:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				return vk::Format::eR32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32Sfloat;
			}
		case TINYGLTF_TYPE_VEC2:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8G8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16G16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: 
				return vk::Format::eR32G32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32G32Sfloat;
			}
		case TINYGLTF_TYPE_VEC3:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8G8B8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16G16B16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				return vk::Format::eR32G32B32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32G32B32Sfloat;
			}
		case TINYGLTF_TYPE_VEC4:
			switch (component_type)
			{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				return vk::Format::eR8G8B8A8Uint;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				return vk::Format::eR16G16B16A16Uint;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				return vk::Format::eR32G32B32A32Uint;
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				return vk::Format::eR32G32B32A32Sfloat;
			}
		}
		assert(false);
		return vk::Format::eUndefined;
	}

	vk::Format toFormat(int gltf_type, int component_num, int bits)
	{
		switch (component_num)
		{
		case 1:
			switch (bits)
			{
			case 8: return vk::Format::eR8Unorm;
			case 16: return vk::Format::eR16Unorm;
			case 32: return vk::Format::eR32Sfloat;
			}
		case 2:
			switch (bits)
			{
			case 8: return vk::Format::eR8G8Unorm;
			case 16: return vk::Format::eR16G16Unorm;
			case 32: return vk::Format::eR32G32Sfloat;
			}
		case 3:
			switch (bits)
			{
			case 8: return vk::Format::eR8G8B8Unorm;
			case 16: return vk::Format::eR16G16B16Unorm;
			case 32: return vk::Format::eR32G32B32Sfloat;
			}
		case 4:
			switch (bits)
			{
			case 8: return vk::Format::eR8G8B8A8Unorm;
			case 16: return vk::Format::eR16G16B16A16Unorm;
			case 32: return vk::Format::eR32G32B32A32Sfloat;
			}
		}
		assert(false);
		return vk::Format::eUndefined;
	}

	vk::Format toFormat(gli::format f)
	{
		// 間違ってる可能性はある
		return (vk::Format)f;
		switch (f)
		{
		case gli::FORMAT_UNDEFINED:
			return vk::Format::eUndefined;
		case gli::FORMAT_RG4_UNORM_PACK8:
			return vk::Format::eR4G4UnormPack8;
		case gli::FORMAT_RGBA4_UNORM_PACK16:
			return vk::Format::eR4G4B4A4UnormPack16;
		case gli::FORMAT_BGRA4_UNORM_PACK16:
			return vk::Format::eB4G4R4A4UnormPack16;
		case gli::FORMAT_R5G6B5_UNORM_PACK16:
			return vk::Format::eR5G6B5UnormPack16;
		case gli::FORMAT_B5G6R5_UNORM_PACK16:
			return vk::Format::eB5G6R5UnormPack16;
		case gli::FORMAT_RGB5A1_UNORM_PACK16:
			return vk::Format::eR5G5B5A1UnormPack16;
		case gli::FORMAT_BGR5A1_UNORM_PACK16:
			return vk::Format::eB5G5R5A1UnormPack16;
		case gli::FORMAT_A1RGB5_UNORM_PACK16:
			return vk::Format::eA1R5G5B5UnormPack16;
		case gli::FORMAT_R8_UNORM_PACK8:
			return vk::Format::eR8Unorm;
		case gli::FORMAT_R8_SNORM_PACK8:
			return vk::Format::eR8Snorm;
		case gli::FORMAT_R8_USCALED_PACK8:
			return vk::Format::eR8Uscaled;
		case gli::FORMAT_R8_SSCALED_PACK8:
			return vk::Format::eR8Sscaled;
		case gli::FORMAT_R8_UINT_PACK8:
			return vk::Format::eR8Uint;
		case gli::FORMAT_R8_SINT_PACK8:
			return vk::Format::eR8Sint;
		case gli::FORMAT_R8_SRGB_PACK8:
			return vk::Format::eR8Srgb;
		case gli::FORMAT_RG8_UNORM_PACK8:
			return vk::Format::eR8G8Unorm;
		case gli::FORMAT_RG8_SNORM_PACK8:
			return vk::Format::eR8G8Snorm;
		case gli::FORMAT_RG8_USCALED_PACK8:
			return vk::Format::eR8G8Uscaled;
		case gli::FORMAT_RG8_SSCALED_PACK8:
			return vk::Format::eR8G8Sscaled;
		case gli::FORMAT_RG8_UINT_PACK8:
			return vk::Format::eR8G8Uint;
		case gli::FORMAT_RG8_SINT_PACK8:
			return vk::Format::eR8G8Sint;
		case gli::FORMAT_RG8_SRGB_PACK8:
			return vk::Format::eR8G8Srgb;
		case gli::FORMAT_RGB8_UNORM_PACK8:
			return vk::Format::eR8G8B8Unorm;
		case gli::FORMAT_RGB8_SNORM_PACK8:
			return vk::Format::eR8G8B8Snorm;
		case gli::FORMAT_RGB8_USCALED_PACK8:
			return vk::Format::eR8G8B8Uscaled;
		case gli::FORMAT_RGB8_SSCALED_PACK8:
			return vk::Format::eR8G8B8Sscaled;
		case gli::FORMAT_RGB8_UINT_PACK8:
			return vk::Format::eR8G8B8Uint;
		case gli::FORMAT_RGB8_SINT_PACK8:
			return vk::Format::eR8G8B8Sint;
		case gli::FORMAT_RGB8_SRGB_PACK8:
			return vk::Format::eR8G8B8Srgb;
		case gli::FORMAT_BGR8_UNORM_PACK8:
			return vk::Format::eB8G8R8Unorm;
		case gli::FORMAT_BGR8_SNORM_PACK8:
			return vk::Format::eB8G8R8Snorm;
		case gli::FORMAT_BGR8_USCALED_PACK8:
			return vk::Format::eB8G8R8Uscaled;
		case gli::FORMAT_BGR8_SSCALED_PACK8:
			return vk::Format::eB8G8R8Sscaled;
		case gli::FORMAT_BGR8_UINT_PACK8:
			return vk::Format::eB8G8R8Uint;
		case gli::FORMAT_BGR8_SINT_PACK8:
			return vk::Format::eB8G8R8Sint;
		case gli::FORMAT_BGR8_SRGB_PACK8:
			return vk::Format::eB8G8R8Srgb;
		case gli::FORMAT_RGBA8_UNORM_PACK8:
			return vk::Format::eB8G8R8Unorm;
		case gli::FORMAT_RGBA8_SNORM_PACK8:
			return vk::Format::eR8G8B8Snorm;
		case gli::FORMAT_RGBA8_USCALED_PACK8:
			return vk::Format::eR8G8B8A8Uscaled;
		case gli::FORMAT_RGBA8_SSCALED_PACK8:
			break;
		case gli::FORMAT_RGBA8_UINT_PACK8:
			break;
		case gli::FORMAT_RGBA8_SINT_PACK8:
			break;
		case gli::FORMAT_RGBA8_SRGB_PACK8:
			break;
		case gli::FORMAT_BGRA8_UNORM_PACK8:
			break;
		case gli::FORMAT_BGRA8_SNORM_PACK8:
			break;
		case gli::FORMAT_BGRA8_USCALED_PACK8:
			break;
		case gli::FORMAT_BGRA8_SSCALED_PACK8:
			break;
		case gli::FORMAT_BGRA8_UINT_PACK8:
			break;
		case gli::FORMAT_BGRA8_SINT_PACK8:
			break;
		case gli::FORMAT_BGRA8_SRGB_PACK8:
			break;
		case gli::FORMAT_RGBA8_UNORM_PACK32:
			break;
		case gli::FORMAT_RGBA8_SNORM_PACK32:
			break;
		case gli::FORMAT_RGBA8_USCALED_PACK32:
			break;
		case gli::FORMAT_RGBA8_SSCALED_PACK32:
			break;
		case gli::FORMAT_RGBA8_UINT_PACK32:
			break;
		case gli::FORMAT_RGBA8_SINT_PACK32:
			break;
		case gli::FORMAT_RGBA8_SRGB_PACK32:
			break;
		case gli::FORMAT_RGB10A2_UNORM_PACK32:
			break;
		case gli::FORMAT_RGB10A2_SNORM_PACK32:
			break;
		case gli::FORMAT_RGB10A2_USCALED_PACK32:
			break;
		case gli::FORMAT_RGB10A2_SSCALED_PACK32:
			break;
		case gli::FORMAT_RGB10A2_UINT_PACK32:
			break;
		case gli::FORMAT_RGB10A2_SINT_PACK32:
			break;
		case gli::FORMAT_BGR10A2_UNORM_PACK32:
			break;
		case gli::FORMAT_BGR10A2_SNORM_PACK32:
			break;
		case gli::FORMAT_BGR10A2_USCALED_PACK32:
			break;
		case gli::FORMAT_BGR10A2_SSCALED_PACK32:
			break;
		case gli::FORMAT_BGR10A2_UINT_PACK32:
			break;
		case gli::FORMAT_BGR10A2_SINT_PACK32:
			break;
		case gli::FORMAT_R16_UNORM_PACK16:
			break;
		case gli::FORMAT_R16_SNORM_PACK16:
			break;
		case gli::FORMAT_R16_USCALED_PACK16:
			break;
		case gli::FORMAT_R16_SSCALED_PACK16:
			break;
		case gli::FORMAT_R16_UINT_PACK16:
			break;
		case gli::FORMAT_R16_SINT_PACK16:
			break;
		case gli::FORMAT_R16_SFLOAT_PACK16:
			break;
		case gli::FORMAT_RG16_UNORM_PACK16:
			break;
		case gli::FORMAT_RG16_SNORM_PACK16:
			break;
		case gli::FORMAT_RG16_USCALED_PACK16:
			break;
		case gli::FORMAT_RG16_SSCALED_PACK16:
			break;
		case gli::FORMAT_RG16_UINT_PACK16:
			break;
		case gli::FORMAT_RG16_SINT_PACK16:
			break;
		case gli::FORMAT_RG16_SFLOAT_PACK16:
			break;
		case gli::FORMAT_RGB16_UNORM_PACK16:
			break;
		case gli::FORMAT_RGB16_SNORM_PACK16:
			break;
		case gli::FORMAT_RGB16_USCALED_PACK16:
			break;
		case gli::FORMAT_RGB16_SSCALED_PACK16:
			break;
		case gli::FORMAT_RGB16_UINT_PACK16:
			break;
		case gli::FORMAT_RGB16_SINT_PACK16:
			break;
		case gli::FORMAT_RGB16_SFLOAT_PACK16:
			break;
		case gli::FORMAT_RGBA16_UNORM_PACK16:
			break;
		case gli::FORMAT_RGBA16_SNORM_PACK16:
			break;
		case gli::FORMAT_RGBA16_USCALED_PACK16:
			break;
		case gli::FORMAT_RGBA16_SSCALED_PACK16:
			break;
		case gli::FORMAT_RGBA16_UINT_PACK16:
			break;
		case gli::FORMAT_RGBA16_SINT_PACK16:
			break;
		case gli::FORMAT_RGBA16_SFLOAT_PACK16:
			break;
		case gli::FORMAT_R32_UINT_PACK32:
			break;
		case gli::FORMAT_R32_SINT_PACK32:
			break;
		case gli::FORMAT_R32_SFLOAT_PACK32:
			break;
		case gli::FORMAT_RG32_UINT_PACK32:
			break;
		case gli::FORMAT_RG32_SINT_PACK32:
			break;
		case gli::FORMAT_RG32_SFLOAT_PACK32:
			break;
		case gli::FORMAT_RGB32_UINT_PACK32:
			break;
		case gli::FORMAT_RGB32_SINT_PACK32:
			break;
		case gli::FORMAT_RGB32_SFLOAT_PACK32:
			break;
		case gli::FORMAT_RGBA32_UINT_PACK32:
			break;
		case gli::FORMAT_RGBA32_SINT_PACK32:
			break;
		case gli::FORMAT_RGBA32_SFLOAT_PACK32:
			break;
		case gli::FORMAT_R64_UINT_PACK64:
			break;
		case gli::FORMAT_R64_SINT_PACK64:
			break;
		case gli::FORMAT_R64_SFLOAT_PACK64:
			break;
		case gli::FORMAT_RG64_UINT_PACK64:
			break;
		case gli::FORMAT_RG64_SINT_PACK64:
			break;
		case gli::FORMAT_RG64_SFLOAT_PACK64:
			break;
		case gli::FORMAT_RGB64_UINT_PACK64:
			break;
		case gli::FORMAT_RGB64_SINT_PACK64:
			break;
		case gli::FORMAT_RGB64_SFLOAT_PACK64:
			break;
		case gli::FORMAT_RGBA64_UINT_PACK64:
			break;
		case gli::FORMAT_RGBA64_SINT_PACK64:
			break;
		case gli::FORMAT_RGBA64_SFLOAT_PACK64:
			break;
		case gli::FORMAT_RG11B10_UFLOAT_PACK32:
			break;
		case gli::FORMAT_RGB9E5_UFLOAT_PACK32:
			break;
		case gli::FORMAT_D16_UNORM_PACK16:
			break;
		case gli::FORMAT_D24_UNORM_PACK32:
			break;
		case gli::FORMAT_D32_SFLOAT_PACK32:
			break;
		case gli::FORMAT_S8_UINT_PACK8:
			break;
		case gli::FORMAT_D16_UNORM_S8_UINT_PACK32:
			break;
		case gli::FORMAT_D24_UNORM_S8_UINT_PACK32:
			break;
		case gli::FORMAT_D32_SFLOAT_S8_UINT_PACK64:
			break;
		case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGB_DXT1_SRGB_BLOCK8:
			break;
		case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:
			break;
		case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:
			break;
		case gli::FORMAT_R_ATI1N_UNORM_BLOCK8:
			break;
		case gli::FORMAT_R_ATI1N_SNORM_BLOCK8:
			break;
		case gli::FORMAT_RG_ATI2N_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RG_ATI2N_SNORM_BLOCK16:
			break;
		case gli::FORMAT_RGB_BP_UFLOAT_BLOCK16:
			break;
		case gli::FORMAT_RGB_BP_SFLOAT_BLOCK16:
			break;
		case gli::FORMAT_RGBA_BP_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_BP_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGB_ETC2_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGB_ETC2_SRGB_BLOCK8:
			break;
		case gli::FORMAT_RGBA_ETC2_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGBA_ETC2_SRGB_BLOCK8:
			break;
		case gli::FORMAT_RGBA_ETC2_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ETC2_SRGB_BLOCK16:
			break;
		case gli::FORMAT_R_EAC_UNORM_BLOCK8:
			break;
		case gli::FORMAT_R_EAC_SNORM_BLOCK8:
			break;
		case gli::FORMAT_RG_EAC_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RG_EAC_SNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_4X4_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_4X4_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_5X4_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_5X4_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_5X5_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_5X5_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_6X5_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_6X5_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_6X6_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_6X6_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_8X5_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_8X5_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_8X6_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_8X6_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_8X8_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_8X8_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X5_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X5_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X6_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X6_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X8_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X8_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X10_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_10X10_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_12X10_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_12X10_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_12X12_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ASTC_12X12_SRGB_BLOCK16:
			break;
		case gli::FORMAT_RGB_PVRTC1_8X8_UNORM_BLOCK32:
			break;
		case gli::FORMAT_RGB_PVRTC1_8X8_SRGB_BLOCK32:
			break;
		case gli::FORMAT_RGB_PVRTC1_16X8_UNORM_BLOCK32:
			break;
		case gli::FORMAT_RGB_PVRTC1_16X8_SRGB_BLOCK32:
			break;
		case gli::FORMAT_RGBA_PVRTC1_8X8_UNORM_BLOCK32:
			break;
		case gli::FORMAT_RGBA_PVRTC1_8X8_SRGB_BLOCK32:
			break;
		case gli::FORMAT_RGBA_PVRTC1_16X8_UNORM_BLOCK32:
			break;
		case gli::FORMAT_RGBA_PVRTC1_16X8_SRGB_BLOCK32:
			break;
		case gli::FORMAT_RGBA_PVRTC2_4X4_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGBA_PVRTC2_4X4_SRGB_BLOCK8:
			break;
		case gli::FORMAT_RGBA_PVRTC2_8X4_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGBA_PVRTC2_8X4_SRGB_BLOCK8:
			break;
		case gli::FORMAT_RGB_ETC_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGB_ATC_UNORM_BLOCK8:
			break;
		case gli::FORMAT_RGBA_ATCA_UNORM_BLOCK16:
			break;
		case gli::FORMAT_RGBA_ATCI_UNORM_BLOCK16:
			break;
		case gli::FORMAT_L8_UNORM_PACK8:
			break;
		case gli::FORMAT_A8_UNORM_PACK8:
			break;
		case gli::FORMAT_LA8_UNORM_PACK8:
			break;
		case gli::FORMAT_L16_UNORM_PACK16:
			break;
		case gli::FORMAT_A16_UNORM_PACK16:
			break;
		case gli::FORMAT_LA16_UNORM_PACK16:
			break;
		case gli::FORMAT_BGR8_UNORM_PACK32:
			break;
		case gli::FORMAT_BGR8_SRGB_PACK32:
			break;
		case gli::FORMAT_RG3B2_UNORM_PACK8:
			break;
		default:
			break;
		}

		assert(false);
		return vk::Format::eUndefined;
	}
}

struct Image
{
	std::string m_filename;
	vk::UniqueImage m_image;
	vk::UniqueImageView m_image_view;
	vk::UniqueDeviceMemory m_memory;
	vk::UniqueSampler m_sampler;

	vk::DescriptorImageInfo info()
	{
		return vk::DescriptorImageInfo().setSampler(m_sampler.get()).setImageView(m_image_view.get()).setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
	}
};

struct BRDFLookupTable
{
	Image m_brgf_lut;

	BRDFLookupTable(btr::Context& ctx, vk::CommandBuffer cmd)
	{
		const vk::Format format = vk::Format::eR16G16Sfloat;
		const int32_t dim = 512;

		// Image
		vk::ImageCreateInfo image_info;
		image_info.imageType = vk::ImageType::e2D;
		image_info.format = format;
		image_info.mipLevels = 1;
		image_info.arrayLayers = 1;
		image_info.samples = vk::SampleCountFlagBits::e1;
		image_info.tiling = vk::ImageTiling::eOptimal;
		image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
		image_info.sharingMode = vk::SharingMode::eExclusive;
		image_info.initialLayout = vk::ImageLayout::eUndefined;
//		image_info.flags
		image_info.extent = vk::Extent3D(dim, dim, 1);

		vk::UniqueImage image = ctx.m_device.createImageUnique(image_info);

		vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(image.get());
		vk::MemoryAllocateInfo memory_alloc_info;
		memory_alloc_info.allocationSize = memory_request.size;
		memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::UniqueDeviceMemory image_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
		ctx.m_device.bindImageMemory(image.get(), image_memory.get(), 0);

		// View
		vk::ImageViewCreateInfo view_info;
		view_info.viewType = vk::ImageViewType::e2D;
		view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
		view_info.flags = vk::ImageViewCreateFlags();
		view_info.format = image_info.format;
		view_info.image = image.get();
		view_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		view_info.subresourceRange.baseArrayLayer = 0;
		view_info.subresourceRange.baseMipLevel = 0;
		view_info.subresourceRange.layerCount = 1;
		view_info.subresourceRange.levelCount = 1;

		// Sampler
		vk::SamplerCreateInfo sampler_info;
		sampler_info.magFilter = vk::Filter::eLinear;
		sampler_info.minFilter = vk::Filter::eLinear;
		sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
		sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
		sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
		sampler_info.mipLodBias = 0.0f;
		sampler_info.compareOp = vk::CompareOp::eNever;
		sampler_info.minLod = 0.f;
		sampler_info.maxLod = 0.f;
		sampler_info.maxAnisotropy = 1.0;
		sampler_info.anisotropyEnable = VK_FALSE;

		m_brgf_lut.m_image_view = ctx.m_device.createImageViewUnique(view_info);
		m_brgf_lut.m_sampler = ctx.m_device.createSamplerUnique(sampler_info);

		m_brgf_lut.m_image = std::move(image);
		m_brgf_lut.m_memory = std::move(image_memory);
		//		m_brgf_lut.m_image_view = std::move(image_view);
//		m_brgf_lut.m_sampler = std::move(sampler);

		// FB, Att, RP, Pipe, etc.
		{
			// レンダーパス
			// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);

			// Use subpass dependencies for layout transitions
			std::array<vk::SubpassDependency, 2> dependencies;
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

			// Create the actual renderpass
			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setDependencies(dependencies);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);


			auto m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);

// 			vk::ImageView view[] =
// 			{
// 				m_brgf_lut.m_image_view.get(),
// 			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_renderpass.get());
			framebuffer_info.setAttachmentCount(1);
//			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(dim);
			framebuffer_info.setHeight(dim);
			framebuffer_info.setLayers(1);
			framebuffer_info.setFlags(vk::FramebufferCreateFlagBits::eImageless);

			vk::FramebufferAttachmentImageInfo framebuffer_image_info;
			framebuffer_image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment);
			framebuffer_image_info.setLayerCount(1);
			framebuffer_image_info.setViewFormatCount(1);
			framebuffer_image_info.setPViewFormats(&format);
			framebuffer_image_info.setWidth(dim);
			framebuffer_image_info.setHeight(dim);
			vk::FramebufferAttachmentsCreateInfo framebuffer_attach_info;
			framebuffer_attach_info.setAttachmentImageInfoCount(1);
			framebuffer_attach_info.setPAttachmentImageInfos(&framebuffer_image_info);

			framebuffer_info.setPNext(&framebuffer_attach_info);

			auto m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);

			// descriptor set layout
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			auto DSL = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);

			// descriptor set
			vk::DescriptorSetLayout layouts[] =
			{
				DSL.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
//			auto DS = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			auto DS = std::move(ctx.m_device.allocateDescriptorSets(desc_info)[0]);

			vk::DescriptorImageInfo images[] =
			{
				m_brgf_lut.info(),
			};

			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet()
				.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
				.setDescriptorCount(array_length(images))
				.setPImageInfo(images)
				.setDstBinding(0)
				.setDstSet(DS),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

			// pipeline layout
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
			pipeline_layout_info.setSetLayoutCount(array_length(layouts));
			pipeline_layout_info.setPSetLayouts(layouts);
			auto PL = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);


			// Pipeline
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"BRDFLUT_Make.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"BRDFLUT_Make.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport[] = { vk::Viewport(0.f, 0.f, dim, dim, 0.f, 1.f) };
			vk::Rect2D scissor[] = { vk::Rect2D(vk::Offset2D(0, 0), dim) };
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);


			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_FALSE);


			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(PL.get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			auto pipeline = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

			// Render
			vk::RenderPassBeginInfo begin_render_Info;
			begin_render_Info.setRenderPass(m_renderpass.get());
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), dim));
			begin_render_Info.setFramebuffer(m_framebuffer.get());

			vk::RenderPassAttachmentBeginInfo attachment_begin_info;
			std::array<vk::ImageView, 1> views = { m_brgf_lut.m_image_view.get() };
			attachment_begin_info.setAttachments(views);

			begin_render_Info.pNext = &attachment_begin_info;

			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, PL.get(), 0, { DS }, {});
			cmd.draw(3, 1, 0, 0);
			cmd.endRenderPass();
		}
	}
};

struct Environment
{
	enum Target { IRRADIANCE = 0, PREFILTEREDENV = 1, Target_Max, };
	vk::UniqueRenderPass m_renderpass;
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS;
	vk::UniquePipelineLayout m_pipeline_layout;
	vk::UniquePipeline m_pipeline;

	Environment(btr::Context& ctx, vk::CommandBuffer cmd)
	{

		for (uint32_t target = 0; target < PREFILTEREDENV + 1; target++)
		{
			//			vks::TextureCubeMap cubemap;
			auto tStart = std::chrono::high_resolution_clock::now();

			vk::Format format;
			int32_t dim;

			switch (target) 
			{
			case IRRADIANCE:
				format = vk::Format::eR32G32B32A32Sfloat;
				dim = 64;
				break;
			case PREFILTEREDENV:
				format = vk::Format::eR16G16B16A16Sfloat;
				dim = 512;
				break;
			};
			const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

			Image image;
			// Create target cubemap
			{
				// Image
				vk::ImageCreateInfo imageCI{};
				imageCI.imageType = vk::ImageType::e2D;
				imageCI.format = format;
				imageCI.extent.width = dim;
				imageCI.extent.height = dim;
				imageCI.extent.depth = 1;
				imageCI.mipLevels = numMips;
				imageCI.arrayLayers = 6;
				imageCI.samples = vk::SampleCountFlagBits::e1;
				imageCI.tiling = vk::ImageTiling::eOptimal;
				imageCI.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
				imageCI.flags = vk::ImageCreateFlagBits::eCubeCompatible;

				image.m_image = ctx.m_device.createImageUnique(imageCI);

				vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(image.m_image.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				image.m_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
				ctx.m_device.bindImageMemory(image.m_image.get(), image.m_memory.get(), 0);

				// View
				vk::ImageViewCreateInfo viewCI{};
				viewCI.viewType = vk::ImageViewType::eCube;
				viewCI.format = format;
				viewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				viewCI.subresourceRange.levelCount = numMips;
				viewCI.subresourceRange.layerCount = 6;
				viewCI.image = image.m_image.get();
				image.m_image_view = ctx.m_device.createImageViewUnique(viewCI);

				// Sampler
				vk::SamplerCreateInfo sampler_info;
				sampler_info.magFilter = vk::Filter::eNearest;
				sampler_info.minFilter = vk::Filter::eNearest;
				sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
				sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
				sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
				sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
				sampler_info.mipLodBias = 0.0f;
				sampler_info.compareOp = vk::CompareOp::eNever;
				sampler_info.minLod = 0.0f;
				sampler_info.maxLod = numMips;
				sampler_info.maxAnisotropy = 1.0;
				sampler_info.anisotropyEnable = VK_FALSE;
				sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;
				image.m_sampler = ctx.m_device.createSamplerUnique(sampler_info);
			}

			// FB, Att, RP, Pipe, etc.

				// sub pass
			vk::AttachmentReference color_ref[] =
			{
				vk::AttachmentReference()
				.setAttachment(0)
				.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
			};

			// Use subpass dependencies for layout transitions
			std::array<vk::SubpassDependency, 2> dependencies;
			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[0].dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[0].srcAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[0].dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[0].dependencyFlags = vk::DependencyFlagBits::eByRegion;
			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			dependencies[1].dstStageMask = vk::PipelineStageFlagBits::eBottomOfPipe;
			dependencies[1].srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
			dependencies[1].dstAccessMask = vk::AccessFlagBits::eMemoryRead;
			dependencies[1].dependencyFlags = vk::DependencyFlagBits::eByRegion;

			vk::SubpassDescription subpass;
			subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
			subpass.setInputAttachmentCount(0);
			subpass.setPInputAttachments(nullptr);
			subpass.setColorAttachmentCount(array_length(color_ref));
			subpass.setPColorAttachments(color_ref);
			subpass.setPDepthStencilAttachment(nullptr);

			vk::AttachmentDescription attach_description[] =
			{
				// color1
				vk::AttachmentDescription()
				.setFormat(format)
				.setSamples(vk::SampleCountFlagBits::e1)
				.setLoadOp(vk::AttachmentLoadOp::eLoad)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
				.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);
			renderpass_info.setDependencies(dependencies);
			auto renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);

			vk::ImageView view[] =
			{
				image.m_image_view.get(),
			};
			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(renderpass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(dim);
			framebuffer_info.setHeight(dim);
			framebuffer_info.setLayers(1);
			auto framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);


			struct Offscreen 
			{
				vk::Image image;
				vk::ImageView view;
				vk::DeviceMemory memory;
				vk::Framebuffer framebuffer;
			} offscreen;

			// Create offscreen framebuffer
			{
				// Image
				vk::ImageCreateInfo imageCI{};
				imageCI.imageType = vk::ImageType::e2D;
				imageCI.format = format;
				imageCI.extent.width = dim;
				imageCI.extent.height = dim;
				imageCI.extent.depth = 1;
				imageCI.mipLevels = 1;
				imageCI.arrayLayers = 1;
				imageCI.samples = vk::SampleCountFlagBits::e1;
				imageCI.tiling = vk::ImageTiling::eOptimal;
				imageCI.initialLayout = vk::ImageLayout::eUndefined;
				imageCI.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
				offscreen.image = ctx.m_device.createImage(imageCI);


				// View
				vk::ImageViewCreateInfo viewCI{};
				viewCI.viewType = vk::ImageViewType::e2D;
				viewCI.format = format;
				viewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				viewCI.subresourceRange.baseMipLevel = 0;
				viewCI.subresourceRange.levelCount = 1;
				viewCI.subresourceRange.baseArrayLayer = 0;
				viewCI.subresourceRange.layerCount = 1;
				viewCI.image = offscreen.image;
				offscreen.view = ctx.m_device.createImageView(viewCI);

				// Framebuffer
				vk::FramebufferCreateInfo framebufferCI;
				framebufferCI.renderPass = m_renderpass.get();
				framebufferCI.attachmentCount = 1;
				framebufferCI.pAttachments = &offscreen.view;
				framebufferCI.width = dim;
				framebufferCI.height = dim;
				framebufferCI.layers = 1;
				offscreen.framebuffer = ctx.m_device.createFramebuffer(framebufferCI);

				vk::ImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.image = offscreen.image;
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
				imageMemoryBarrier.srcAccessMask = {};
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
				imageMemoryBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
			}

			// Descriptors
			vk::DescriptorSetLayoutBinding setLayoutBinding = vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment, nullptr };
			vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
			descriptorSetLayoutCI.pBindings = &setLayoutBinding;
			descriptorSetLayoutCI.bindingCount = 1;
			vk::DescriptorSetLayout descriptorsetlayout = ctx.m_device.createDescriptorSetLayout(descriptorSetLayoutCI);

			// Descriptor sets
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL.get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS = std::move(ctx.m_device.allocateDescriptorSetsUnique(desc_info)[0]);

			vk::DescriptorImageInfo images[] = {
				image.info(),
			};
			vk::WriteDescriptorSet write[] =
			{
				vk::WriteDescriptorSet().setDstSet(*m_DS).setDstBinding(0).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(array_length(images)).setPImageInfo(images),
			};
			ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

			struct PushBlockIrradiance
			{
				mat4 mvp;
				float deltaPhi = (2.0f * glm::pi<float>()) / 180.0f;
				float deltaTheta = (0.5f * glm::pi<float>()) / 64.0f;
			} pushBlockIrradiance;

			struct PushBlockPrefilterEnv
			{
				mat4 mvp;
				float roughness;
				uint32_t numSamples = 32u;
			} pushBlockPrefilterEnv;

			// Pipeline layout
			vk::PushConstantRange pushConstantRange{};
			pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

			switch (target)
			{
			case IRRADIANCE:
				pushConstantRange.size = sizeof(PushBlockIrradiance);
				break;
			case PREFILTEREDENV:
				pushConstantRange.size = sizeof(PushBlockPrefilterEnv);
				break;
			};

			vk::PipelineLayoutCreateInfo pipelineLayoutCI{};
			pipelineLayoutCI.setLayoutCount = 1;
			pipelineLayoutCI.pSetLayouts = &descriptorsetlayout;
			pipelineLayoutCI.pushConstantRangeCount = 1;
			pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
			m_pipeline_layout = ctx.m_device.createPipelineLayoutUnique(pipelineLayoutCI);

			// Pipeline
			vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI;
			inputAssemblyStateCI.topology = vk::PrimitiveTopology::eTriangleList;

			vk::PipelineRasterizationStateCreateInfo rasterizationStateCI;
			rasterizationStateCI.polygonMode = vk::PolygonMode::eFill;
			rasterizationStateCI.cullMode = vk::CullModeFlagBits::eNone;
			rasterizationStateCI.frontFace = vk::FrontFace::eCounterClockwise;
			rasterizationStateCI.lineWidth = 1.0f;

			vk::PipelineColorBlendAttachmentState blendAttachmentState;
			blendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
			blendAttachmentState.blendEnable = VK_FALSE;

			vk::PipelineColorBlendStateCreateInfo colorBlendStateCI;
			colorBlendStateCI.attachmentCount = 1;
			colorBlendStateCI.pAttachments = &blendAttachmentState;

			vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI;
			depthStencilStateCI.depthTestEnable = VK_FALSE;
			depthStencilStateCI.depthWriteEnable = VK_FALSE;
			depthStencilStateCI.depthCompareOp = vk::CompareOp::eLessOrEqual;
			depthStencilStateCI.front = depthStencilStateCI.back;
			depthStencilStateCI.back.compareOp = vk::CompareOp::eAlways;

			vk::PipelineViewportStateCreateInfo viewportStateCI;
			viewportStateCI.viewportCount = 1;
			viewportStateCI.scissorCount = 1;

			vk::PipelineMultisampleStateCreateInfo multisampleStateCI;
			multisampleStateCI.rasterizationSamples = vk::SampleCountFlagBits::e1;

			std::vector<vk::DynamicState> dynamicStateEnables = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
			vk::PipelineDynamicStateCreateInfo dynamicStateCI;
			dynamicStateCI.setDynamicStates(dynamicStateEnables);

			vk::PipelineVertexInputStateCreateInfo vertexInputStateCI;

			std::array<vk::UniqueShaderModule, 3> shader;
			std::array<vk::PipelineShaderStageCreateInfo, 3> shaderStages;
			shader[0] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "filtercube.vert.spv");
			shaderStages[0].module = shader[0].get();
			shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
			shaderStages[0].pName = "main";
			shader[1] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "filtercube.geom.spv");
			shaderStages[1].module = shader[1].get();
			shaderStages[1].stage = vk::ShaderStageFlagBits::eVertex;
			shaderStages[1].pName = "main";

			switch (target)
			{
			case IRRADIANCE:
				shader[2] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "irradiancecube.frag.spv");
				shaderStages[2].module = shader[2].get();
				shaderStages[2].stage = vk::ShaderStageFlagBits::eFragment;
				shaderStages[2].pName = "main";
				break;
			case PREFILTEREDENV:
				shader[2] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "prefilterenvmap.frag.spv");
				shaderStages[2].module = shader[2].get();
				shaderStages[2].stage = vk::ShaderStageFlagBits::eFragment;
				shaderStages[2].pName = "main";
				break;
			};

			vk::GraphicsPipelineCreateInfo pipelineCI{};
			pipelineCI.layout = m_pipeline_layout.get();
			pipelineCI.renderPass = m_renderpass.get();
			pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
			pipelineCI.pVertexInputState = &vertexInputStateCI;
			pipelineCI.pRasterizationState = &rasterizationStateCI;
			pipelineCI.pColorBlendState = &colorBlendStateCI;
			pipelineCI.pMultisampleState = &multisampleStateCI;
			pipelineCI.pViewportState = &viewportStateCI;
			pipelineCI.pDepthStencilState = &depthStencilStateCI;
			pipelineCI.pDynamicState = &dynamicStateCI;
			pipelineCI.stageCount = array_size(shaderStages);
			pipelineCI.pStages = shaderStages.data();

			m_pipeline = ctx.m_device.createGraphicsPipelineUnique({}, pipelineCI).value;

			// Render cubemap
			vk::ClearValue clearValues[1];
			clearValues[0].color = vk::ClearColorValue{ std::array<float, 4>{0.0f, 0.0f, 0.2f, 0.0f}};

			vk::RenderPassBeginInfo renderPassBeginInfo;
			renderPassBeginInfo.renderPass = m_renderpass.get();
			renderPassBeginInfo.framebuffer = offscreen.framebuffer;
			renderPassBeginInfo.renderArea.extent.width = dim;
			renderPassBeginInfo.renderArea.extent.height = dim;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;

			std::vector<glm::mat4> matrices = 
			{
				glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
				glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
				glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
				glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
				glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
				glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			};

			vk::Viewport viewport{};
			viewport.width = (float)dim;
			viewport.height = (float)dim;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vk::Rect2D scissor{};
			scissor.extent.width = dim;
			scissor.extent.height = dim;

			vk::ImageSubresourceRange subresourceRange{};
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = numMips;
			subresourceRange.layerCount = 6;

			// Change image layout for all cubemap faces to transfer destination
			{
				vk::ImageMemoryBarrier imageMemoryBarrier;
//				imageMemoryBarrier.image = cubemap.image;
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				imageMemoryBarrier.srcAccessMask = {};
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
			}

			for (uint32_t m = 0; m < numMips; m++) 
			{
				for (uint32_t f = 0; f < 6; f++) 
				{

					viewport.width = static_cast<float>(dim * std::pow(0.5f, m));
					viewport.height = static_cast<float>(dim * std::pow(0.5f, m));
					cmd.setViewport(0, 1, &viewport);
					cmd.setScissor(0, 1, &scissor);

					// Render scene from cube face's point of view
					cmd.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

					// Pass parameters for current pass using a push constant block
					switch (target) 
					{
					case IRRADIANCE:
						pushBlockIrradiance.mvp = glm::perspective(glm::half_pi<float>(), 1.0f, 0.1f, 512.0f) * matrices[f];
						cmd.pushConstants(m_pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlockIrradiance), &pushBlockIrradiance);
						break;
					case PREFILTEREDENV:
						pushBlockPrefilterEnv.mvp = glm::perspective(glm::half_pi<float>(), 1.0f, 0.1f, 512.0f) * matrices[f];
						pushBlockPrefilterEnv.roughness = (float)m / (float)(numMips - 1);
						cmd.pushConstants(m_pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlockPrefilterEnv), &pushBlockPrefilterEnv);
						break;
					};

					cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
					cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, { m_DS.get() }, {});

//					vk::DeviceSize offsets[1] = { 0 };
//					models.skybox.draw(cmdBuf);

					cmd.endRenderPass();

					VkImageSubresourceRange subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
					subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					subresourceRange.baseMipLevel = 0;
					subresourceRange.levelCount = numMips;
					subresourceRange.layerCount = 6;

					{
						VkImageMemoryBarrier imageMemoryBarrier{};
						imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
						imageMemoryBarrier.image = offscreen.image;
						imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
						imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
						imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
						imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
						imageMemoryBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
					}

					// Copy region for transfer from framebuffer to cube face
					vk::ImageCopy copyRegion{};

					copyRegion.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
					copyRegion.srcSubresource.baseArrayLayer = 0;
					copyRegion.srcSubresource.mipLevel = 0;
					copyRegion.srcSubresource.layerCount = 1;
					copyRegion.srcOffset = vk::Offset3D{ 0, 0, 0 };

					copyRegion.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
					copyRegion.dstSubresource.baseArrayLayer = f;
					copyRegion.dstSubresource.mipLevel = m;
					copyRegion.dstSubresource.layerCount = 1;
					copyRegion.dstOffset = vk::Offset3D{ 0, 0, 0 };

					copyRegion.extent.width = static_cast<uint32_t>(viewport.width);
					copyRegion.extent.height = static_cast<uint32_t>(viewport.height);
					copyRegion.extent.depth = 1;

//					cmd.copyImage(offscreen.image, vk::ImageLayout::eTransferSrcOptimal, cubemap.image, vk::ImageLayout::eTransferDstOptimal,{ copyRegion });

					{
						vk::ImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.image = offscreen.image;
						imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
						imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
						imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
						imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
						imageMemoryBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
					}
				}
			}

			{
				vk::ImageMemoryBarrier imageMemoryBarrier{};
//				imageMemoryBarrier.image = cubemap.image;
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eHostWrite | vk::AccessFlagBits::eTransferWrite;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
			}


// 			vkDestroyRenderPass(device, renderpass, nullptr);
// 			vkDestroyFramebuffer(device, offscreen.framebuffer, nullptr);
// 			vkFreeMemory(device, offscreen.memory, nullptr);
// 			vkDestroyImageView(device, offscreen.view, nullptr);
// 			vkDestroyImage(device, offscreen.image, nullptr);
// 			vkDestroyDescriptorPool(device, descriptorpool, nullptr);
// 			vkDestroyDescriptorSetLayout(device, descriptorsetlayout, nullptr);
// 			vkDestroyPipeline(device, pipeline, nullptr);
// 			vkDestroyPipelineLayout(device, pipelinelayout, nullptr);
// 
// 			cubemap.descriptor.imageView = cubemap.view;
// 			cubemap.descriptor.sampler = cubemap.sampler;
// 			cubemap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
// 			cubemap.device = vulkanDevice;

			switch (target) 
			{
			case IRRADIANCE:
//				textures.irradianceCube = cubemap;
				break;
			case PREFILTEREDENV:
//				textures.prefilteredCube = cubemap;
//				shaderValuesParams.prefilteredCubeMipLevels = static_cast<float>(numMips);
				break;
			};

// 			auto tEnd = std::chrono::high_resolution_clock::now();
// 			auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
// 			std::cout << "Generating cube map with " << numMips << " mip levels took " << tDiff << " ms" << std::endl;
		}
	}
};
struct Context
{
	enum DSL
	{
		DSL_Scene,
		DSL_Model,
		DSL_Model_Material,
		DSL_Num,
	};

	std::shared_ptr<btr::Context> m_ctx;
	vk::UniqueDescriptorSetLayout m_DSL[DSL_Num];


	struct RenderConfig
	{
		float exposure;
		float gamma;
	};

	BRDFLookupTable m_lut;
	RenderConfig m_render_config;
	btr::BufferMemoryEx<RenderConfig> u_render_config;
	Image m_environment;
	Image m_environment_debug;

	vk::UniqueDescriptorSet m_DS_Scene;

	Context(std::shared_ptr<btr::Context>& ctx)
		: m_lut(*ctx, ctx->m_cmd_pool->allocCmdOnetime(0))
	{
		m_ctx = ctx;

		m_render_config.exposure = 10.f;
		m_render_config.gamma = 2.2f;
		auto cmd = ctx->m_cmd_pool->allocCmdTempolary(0);

		u_render_config = ctx->m_uniform_memory.allocateMemory<RenderConfig>(1);
		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eCombinedImageSampler, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Scene] = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}
			{
				auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Model] = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}

			{
				auto stage = vk::ShaderStageFlagBits::eFragment;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(13, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(14, vk::DescriptorType::eCombinedImageSampler, 1, stage),
				};
				vk::DescriptorSetLayoutCreateInfo desc_layout_info;
				desc_layout_info.setBindingCount(array_length(binding));
				desc_layout_info.setPBindings(binding);
				m_DSL[DSL_Model_Material] = ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
			}
		}

		// environment texture
		{
			gli::texture_cube tex(gli::load(btr::getResourceAppPath() + "environments/papermill.ktx"));

			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = GLtoVK::toFormat(tex.format());
			image_info.mipLevels = tex.levels();
			image_info.arrayLayers = 6;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;
			image_info.extent = vk::Extent3D(tex.extent().x, tex.extent().y, 1);

			vk::UniqueImage image = ctx->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = ctx->m_device.getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			vk::UniqueDeviceMemory image_memory = ctx->m_device.allocateMemoryUnique(memory_alloc_info);
			ctx->m_device.bindImageMemory(image.get(), image_memory.get(), 0);

			auto staging_buffer = ctx->m_staging_memory.allocateMemory<byte>(tex.size(), true);
			memcpy(staging_buffer.getMappedPtr(), tex.data(), tex.size());

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = VK_REMAINING_MIP_LEVELS;
			subresourceRange.levelCount = VK_REMAINING_ARRAY_LAYERS;

			{
				// staging_bufferからimageへコピー
				{
					vk::ImageMemoryBarrier to_copy_barrier;
					to_copy_barrier.image = image.get();
					to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
					to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
					to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_copy_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				}

				std::vector<vk::BufferImageCopy> copys;
				uint offset = staging_buffer.getInfo().offset;
				for (uint32_t face = 0; face < 6; face++)
				{
					for (uint32_t level = 0; level < tex.levels(); level++)
					{
						vk::BufferImageCopy copy = {};
						copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						copy.imageSubresource.mipLevel = level;
						copy.imageSubresource.baseArrayLayer = face;
						copy.imageSubresource.layerCount = 1;
						copy.imageExtent.width = static_cast<uint32_t>(tex[face][level].extent().x);
						copy.imageExtent.height = static_cast<uint32_t>(tex[face][level].extent().y);
						copy.imageExtent.depth = 1;
						copy.bufferOffset = offset;

						copys.push_back(copy);

						// Increase offset into staging buffer for next level / face
						offset += tex[face][level].size();
					}
				}
				cmd.copyBufferToImage(staging_buffer.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copys);

				{
					vk::ImageMemoryBarrier to_shader_read_barrier;
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
			view_info.viewType = vk::ImageViewType::eCube;
			view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eLinear;
			sampler_info.minFilter = vk::Filter::eLinear;
			sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
			sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
			sampler_info.mipLodBias = 0.0f;
			sampler_info.compareOp = vk::CompareOp::eNever;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = image_info.mipLevels;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

			m_environment.m_image = std::move(image);
			m_environment.m_memory = std::move(image_memory);
			m_environment.m_image_view = ctx->m_device.createImageViewUnique(view_info);
			m_environment.m_sampler = ctx->m_device.createSamplerUnique(sampler_info);

		}

		// debug cube map
		{
			vk::ImageCreateInfo image_info;
			image_info.imageType = vk::ImageType::e2D;
			image_info.format = vk::Format::eR32G32B32A32Sfloat;
			image_info.mipLevels = 1;
			image_info.arrayLayers = 6;
			image_info.samples = vk::SampleCountFlagBits::e1;
			image_info.tiling = vk::ImageTiling::eOptimal;
			image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
			image_info.sharingMode = vk::SharingMode::eExclusive;
			image_info.initialLayout = vk::ImageLayout::eUndefined;
			image_info.flags = vk::ImageCreateFlagBits::eCubeCompatible;
			image_info.extent = vk::Extent3D(1, 1, 1);

			vk::UniqueImage image = ctx->m_device.createImageUnique(image_info);

			vk::MemoryRequirements memory_request = ctx->m_device.getImageMemoryRequirements(image.get());
			vk::MemoryAllocateInfo memory_alloc_info;
			memory_alloc_info.allocationSize = memory_request.size;
			memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx->m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

			vk::UniqueDeviceMemory image_memory = ctx->m_device.allocateMemoryUnique(memory_alloc_info);
			ctx->m_device.bindImageMemory(image.get(), image_memory.get(), 0);

			vec4 data[] = 
			{
				vec4(1.f, 0.f, 0.f, 1.f),
				vec4(0.f, 1.f, 0.f, 1.f),
				vec4(0.f, 0.f, 1.f, 1.f),
				vec4(1.f, 1.f, 0.f, 1.f),
				vec4(0.f, 1.f, 1.f, 1.f),
				vec4(1.f, 0.f, 1.f, 1.f),
			};
			auto staging_buffer = ctx->m_staging_memory.allocateMemory<vec4>(6, true);
			memcpy(staging_buffer.getMappedPtr(), data, sizeof(data));

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseArrayLayer = 0;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.layerCount = VK_REMAINING_MIP_LEVELS;
			subresourceRange.levelCount = VK_REMAINING_ARRAY_LAYERS;

			{
				// staging_bufferからimageへコピー
				{
					vk::ImageMemoryBarrier to_copy_barrier;
					to_copy_barrier.image = image.get();
					to_copy_barrier.oldLayout = vk::ImageLayout::eUndefined;
					to_copy_barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
					to_copy_barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
					to_copy_barrier.subresourceRange = subresourceRange;
					cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), {}, {}, { to_copy_barrier });
				}
				std::vector<vk::BufferImageCopy> copys;
				uint offset = staging_buffer.getInfo().offset;
				for (uint32_t face = 0; face < 6; face++)
				{
					for (uint32_t level = 0; level < image_info.mipLevels; level++)
					{
						vk::BufferImageCopy copy = {};
						copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
						copy.imageSubresource.mipLevel = level;
						copy.imageSubresource.baseArrayLayer = face;
						copy.imageSubresource.layerCount = 1;
						copy.imageExtent.width = 1;
						copy.imageExtent.height = 1;
						copy.imageExtent.depth = 1;
						copy.bufferOffset = offset;

						copys.push_back(copy);

						offset += sizeof(vec4);
					}
				}
				cmd.copyBufferToImage(staging_buffer.getInfo().buffer, image.get(), vk::ImageLayout::eTransferDstOptimal, copys);


				{
					vk::ImageMemoryBarrier to_shader_read_barrier;
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
			view_info.viewType = vk::ImageViewType::eCube;
			view_info.components = { vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB,vk::ComponentSwizzle::eA };
			view_info.flags = vk::ImageViewCreateFlags();
			view_info.format = image_info.format;
			view_info.image = image.get();
			view_info.subresourceRange = subresourceRange;

			vk::SamplerCreateInfo sampler_info;
			sampler_info.magFilter = vk::Filter::eNearest;
			sampler_info.minFilter = vk::Filter::eNearest;
			sampler_info.mipmapMode = vk::SamplerMipmapMode::eNearest;
			sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
			sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
			sampler_info.mipLodBias = 0.0f;
			sampler_info.compareOp = vk::CompareOp::eNever;
			sampler_info.minLod = 0.0f;
			sampler_info.maxLod = image_info.mipLevels;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

			m_environment_debug.m_image = std::move(image);
			m_environment_debug.m_memory = std::move(image_memory);
			m_environment_debug.m_image_view = ctx->m_device.createImageViewUnique(view_info);
			m_environment_debug.m_sampler = ctx->m_device.createSamplerUnique(sampler_info);

		}
		{
			vk::DescriptorSetLayout layouts[] =
			{
				m_DSL[Context::DSL_Scene].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			m_DS_Scene = std::move(ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
			{
 				vk::DescriptorBufferInfo uniforms[] =
 				{
					u_render_config.getInfo()
 				};

				vk::DescriptorImageInfo images[] = {
					m_environment.info(),
//					m_environment_debug.info(),
				};
				vk::WriteDescriptorSet write[] =
				{
					vk::WriteDescriptorSet().setDstSet(*m_DS_Scene).setDstBinding(0).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(array_length(uniforms)).setPBufferInfo(uniforms),
					vk::WriteDescriptorSet().setDstSet(*m_DS_Scene).setDstBinding(10).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(array_length(images)).setPImageInfo(images)
				};
				ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

			}
		}
	}




	void execute(vk::CommandBuffer cmd)
	{
		app::g_app_instance->m_window->getImgui()->pushImguiCmd([this]()
			{
				static bool is_open;
				ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowSize(ImVec2(550, 680), ImGuiCond_FirstUseEver);
				if (!ImGui::Begin("ImGui Demo", &is_open, ImGuiWindowFlags_NoSavedSettings))
				{
					// Early out if the window is collapsed, as an optimization.
					ImGui::End();
					return;
				}
				ImGui::SliderFloat("exposure", &this->m_render_config.exposure, 0.0f, 20.f);
//				ImGui::SliderFloat("gamma", &this->m_render_config.gamma, 0.f, 1000.f);
				ImGui::Text("Password input");
				ImGui::End();

			});
		auto staging = m_ctx->m_staging_memory.allocateMemory<RenderConfig>(1, true);
		memcpy_s(staging.getMappedPtr(), sizeof(RenderConfig), &m_render_config, sizeof(RenderConfig));
		vk::BufferCopy copy = vk::BufferCopy(staging.getInfo().offset, u_render_config.getInfo().offset, staging.getInfo().range);
		cmd.copyBuffer(staging.getInfo().buffer, u_render_config.getInfo().buffer, copy);

		{
			vk::BufferMemoryBarrier to_read = u_render_config.makeMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead);
			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllGraphics, {}, {}, {to_read}, { });
		}
	}
};
struct Skybox
{
	vk::UniquePipeline m_pipeline;
	vk::UniquePipelineLayout m_PL;

	vk::UniqueRenderPass m_renderpass;
	vk::UniqueFramebuffer m_framebuffer;

	Skybox(Context& ctx_, vk::CommandBuffer cmd, RenderTarget& rt)
	{
		auto& ctx = *ctx_.m_ctx;
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					ctx_.m_DSL[Context::DSL_Scene].get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL = ctx.m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// graphics pipeline render
		{
			// レンダーパス
			{
				// sub pass
				vk::AttachmentReference color_ref[] =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};
				vk::AttachmentReference depth_ref;
				depth_ref.setAttachment(1);
				depth_ref.setLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(array_length(color_ref));
				subpass.setPColorAttachments(color_ref);
				subpass.setPDepthStencilAttachment(&depth_ref);

				vk::AttachmentDescription attach_description[] =
				{
					// color1
					vk::AttachmentDescription()
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
					// depth
					vk::AttachmentDescription()
					.setFormat(rt.m_depth_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(array_length(attach_description));
				renderpass_info.setPAttachments(attach_description);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);
				m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);
			}

			{
				vk::ImageView view[] =
				{
					rt.m_view,
					rt.m_depth_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_renderpass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(rt.m_info.extent.width);
				framebuffer_info.setHeight(rt.m_info.extent.height);
				framebuffer_info.setLayers(1);
				m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"Skybox.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"Skybox.geom.spv", vk::ShaderStageFlagBits::eGeometry},
				{"Skybox.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::ePointList);

			// viewport
			vk::Viewport viewport[] = { vk::Viewport(0.f, 0.f, rt.m_resolution.width, rt.m_resolution.height, 0.f, 1.f) };
			vk::Rect2D scissor[] = { vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution) };
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);


			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);


			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR| vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL.get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline = ctx.m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}

	}

	void execute_Render(vk::CommandBuffer cmd, Context& ctx, RenderTarget& rt)
	{
		DebugLabel _label(cmd, __FUNCTION__);
		{
			vk::ImageMemoryBarrier image_barrier[1];
			image_barrier[0].setImage(rt.m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline.get());
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL.get(), 1, { ctx.m_DS_Scene.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		cmd.draw(1, 1, 0, 0);
		cmd.endRenderPass();

	}
};



struct Model
{
	struct Info
	{
		vec4 m_aabb_min;
		vec4 m_aabb_max;
		uint m_vertex_num;
		uint m_primitive_num;
		uvec3 m_voxel_reso;
	};

	struct Mesh
	{
		std::vector<vk::VertexInputBindingDescription> vib_disc;
		std::vector<vk::VertexInputAttributeDescription> vid_disc;
	};

	struct Material
	{
		vec4 m_basecolor_factor;
		float m_metallic_factor;
		float m_roughness_factor;
		float _p1;
		float _p2;
		vec3  m_emissive_factor;
		float _p11;
	};
	std::vector<Mesh> m_meshes;
	std::vector<btr::BufferMemory> b_vertex;
	std::vector<ResourceTexture> t_image;
	std::vector<btr::BufferMemoryEx<Material>> b_material;
	std::vector<vk::UniqueDescriptorSet> m_DS_material;

	tinygltf::Model gltf_model;

	vk::UniqueDescriptorSet m_DS_Model;

	Info m_info;

	static std::shared_ptr<Model> LoadModel(Context& ctx, vk::CommandBuffer cmd, const std::string& filename)
	{
		tinygltf::Model gltf_model;
		{
			tinygltf::TinyGLTF loader;
//			loader.SetImageLoader()
			std::string err;
			std::string warn;

//			loader.
			bool res = loader.LoadASCIIFromFile(&gltf_model, &err, &warn, filename);
			if (!warn.empty()) { std::cout << "WARN: " << warn << std::endl; }
			if (!err.empty()) { std::cout << "ERR: " << err << std::endl; }

			if (!res) std::cout << "Failed to load glTF: " << filename << std::endl;
			else std::cout << "Loaded glTF: " << filename << std::endl;
		}

		auto model = std::make_shared<Model>();

		// buffer
		{
			std::vector<vk::DescriptorBufferInfo> info(gltf_model.buffers.size());
			uint buffersize_total = 0;
			model->b_vertex.resize(gltf_model.buffers.size());
			std::vector<btr::BufferMemory> staging(gltf_model.buffers.size());
			for (int i = 0; i < gltf_model.buffers.size(); i++)
			{
				model->b_vertex[i] = ctx.m_ctx->m_vertex_memory.allocateMemory(gltf_model.buffers[i].data.size());
				staging[i] = ctx.m_ctx->m_staging_memory.allocateMemory(gltf_model.buffers[i].data.size());
				memcpy_s(staging[i].getMappedPtr(), gltf_model.buffers[i].data.size(), gltf_model.buffers[i].data.data(), gltf_model.buffers[i].data.size());

				vk::BufferCopy copy = vk::BufferCopy().setSize(gltf_model.buffers[i].data.size()).setSrcOffset(staging[i].getInfo().offset).setDstOffset(model->b_vertex[i].getInfo().offset);
				cmd.copyBuffer(staging[i].getInfo().buffer, model->b_vertex[i].getInfo().buffer, copy);

				info[i].offset = info[std::max(i - 1, 0)].offset + info[std::max(i - 1, 0)].range;
				buffersize_total += gltf_model.buffers[i].data.size();
			}
		}

		// image 
		{
			model->t_image.resize(gltf_model.images.size());
			for (size_t i = 0; i < gltf_model.images.size(); i++)
			{
				tinygltf::Texture& tex = gltf_model.textures[i];
				tinygltf::Image& image = gltf_model.images[i];
//				image.pixel_type
				vk::ImageCreateInfo image_info;
				image_info.imageType = vk::ImageType::e2D;
				image_info.format = GLtoVK::toFormat(image.pixel_type, image.component, image.bits);
				image_info.mipLevels = 1;
				image_info.arrayLayers = 1;
				image_info.samples = vk::SampleCountFlagBits::e1;
				image_info.tiling = vk::ImageTiling::eLinear;
				image_info.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
				image_info.sharingMode = vk::SharingMode::eExclusive;
				image_info.initialLayout = vk::ImageLayout::eUndefined;
				image_info.extent = vk::Extent3D(image.width, image.height, 1);

				model->t_image[i].make(*ctx.m_ctx, cmd, image.uri, image_info, image.image);
			}
		}
		// material
		{
			model->b_material.resize(gltf_model.materials.size());
			model->m_DS_material.resize(gltf_model.materials.size());
			vk::DescriptorSetLayout layouts[] =
			{
				ctx.m_DSL[Context::DSL_Model_Material].get(),
			};
			vk::DescriptorSetAllocateInfo desc_info;
			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
			desc_info.setDescriptorSetCount(array_length(layouts));
			desc_info.setPSetLayouts(layouts);
			
			for (size_t i = 0; i < gltf_model.materials.size(); i++)
			{
				auto gltf_material = gltf_model.materials[i];

				auto ms = ctx.m_ctx->m_staging_memory.allocateMemory<Material>(1, true);
				Material& material = ms.getMappedPtr()[i];

//				gltf_material.alphaCutoff

				material.m_basecolor_factor.x = gltf_material.pbrMetallicRoughness.baseColorFactor[0];
				material.m_basecolor_factor.y = gltf_material.pbrMetallicRoughness.baseColorFactor[1];
				material.m_basecolor_factor.z = gltf_material.pbrMetallicRoughness.baseColorFactor[2];
				material.m_basecolor_factor.w = gltf_material.pbrMetallicRoughness.baseColorFactor[3];
				material.m_metallic_factor = gltf_material.pbrMetallicRoughness.metallicFactor;
				material.m_roughness_factor = gltf_material.pbrMetallicRoughness.roughnessFactor;
				material.m_emissive_factor.x = gltf_material.emissiveFactor[0];
				material.m_emissive_factor.y = gltf_material.emissiveFactor[1];
				material.m_emissive_factor.z = gltf_material.emissiveFactor[2];
				model->b_material[i] = ctx.m_ctx->m_uniform_memory.allocateMemory<Material>(1);
				vk::BufferCopy copy = vk::BufferCopy().setSrcOffset(ms.getInfo().offset).setSize(ms.getInfo().range).setDstOffset(model->b_material[i].getInfo().offset);
				cmd.copyBuffer(ms.getInfo().buffer, model->b_material[i].getInfo().buffer, copy);

				// descriptor set
				{
	
					model->m_DS_material[i] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);

					vk::DescriptorBufferInfo uniforms[] =
					{
						model->b_material[i].getInfo(),
					};
					std::array<vk::DescriptorImageInfo, 5> samplers;
					auto& t = sGraphicsResource::Order().getWhiteTexture();
					samplers.fill(vk::DescriptorImageInfo(t.m_sampler.get(), t.m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));

					if (gltf_material.pbrMetallicRoughness.baseColorTexture.index >= 0)
					{
						samplers[0].sampler = model->t_image[gltf_material.pbrMetallicRoughness.baseColorTexture.index].getSampler();
						samplers[0].imageView = model->t_image[gltf_material.pbrMetallicRoughness.baseColorTexture.index].getImageView();
						samplers[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}
					if (gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
					{
						samplers[1].sampler = model->t_image[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index].getSampler();
						samplers[1].imageView = model->t_image[gltf_material.pbrMetallicRoughness.metallicRoughnessTexture.index].getImageView();
						samplers[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}
					if (gltf_material.normalTexture.index >= 0)
					{
						samplers[2].sampler = model->t_image[gltf_material.normalTexture.index].getSampler();
						samplers[2].imageView = model->t_image[gltf_material.normalTexture.index].getImageView();
						samplers[2].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}

					if (gltf_material.occlusionTexture.index >= 0)
					{
						samplers[3].sampler = model->t_image[gltf_material.occlusionTexture.index].getSampler();
						samplers[3].imageView = model->t_image[gltf_material.occlusionTexture.index].getImageView();
						samplers[3].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}
					if (gltf_material.emissiveTexture.index >= 0)
					{
						samplers[4].sampler = model->t_image[gltf_material.emissiveTexture.index].getSampler();
						samplers[4].imageView = model->t_image[gltf_material.emissiveTexture.index].getImageView();
						samplers[4].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
					}

					vk::WriteDescriptorSet write[] =
					{
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eUniformBuffer)
						.setDescriptorCount(array_length(uniforms))
						.setPBufferInfo(uniforms)
						.setDstBinding(0)
						.setDstSet(model->m_DS_material[i].get()),
						vk::WriteDescriptorSet()
						.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
						.setDescriptorCount(array_length(samplers))
						.setPImageInfo(samplers.data())
						.setDstBinding(10)
						.setDstSet(model->m_DS_material[i].get()),
					};
					ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);
				}
			}


		}
		model->m_meshes.resize(gltf_model.meshes.size());
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			auto& gltf_mesh = gltf_model.meshes[mesh_index];
			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i) 
			{
				tinygltf::Primitive primitive = gltf_mesh.primitives[i];
				tinygltf::Accessor indexAccessor = gltf_model.accessors[primitive.indices];
				tinygltf::BufferView bufferview_index = gltf_model.bufferViews[indexAccessor.bufferView];

				for (auto& attrib : primitive.attributes) 
				{
					tinygltf::Accessor accessor = gltf_model.accessors[attrib.second];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					int size = 1;
					if (accessor.type != TINYGLTF_TYPE_SCALAR) 
					{
						size = accessor.type;
					}

					int vaa = -1;
					if (attrib.first.compare("POSITION") == 0) vaa = 0;
					if (attrib.first.compare("NORMAL") == 0) vaa = 1;
					if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
					if (vaa > -1) 
					{
						model->m_meshes[mesh_index].vib_disc.push_back(vk::VertexInputBindingDescription().setBinding(vaa).setInputRate(vk::VertexInputRate::eVertex).setStride(accessor.ByteStride(bufferview)));
						model->m_meshes[mesh_index].vid_disc.push_back(vk::VertexInputAttributeDescription().setBinding(vaa).setLocation(vaa).setFormat(GLtoVK::toFormat(size, accessor.componentType)).setOffset(accessor.byteOffset));
					}
					else
					{
						std::cout << "vaa missing: " << attrib.first << std::endl;
					}
				}

			}
		}

		model->gltf_model = std::move(gltf_model);
		return model;
	}
};

struct ModelRenderer
{
	enum
	{
		DSL_Renderer,
		DSL_Num,
	};
	enum
	{
		DS_Renderer,
		DS_Num,
	};

	enum
	{
		Pipeline_Render,
		Pipeline_Num,
	};
	enum
	{
		PipelineLayout_Render,
		PipelineLayout_Num,
	};

	std::array<vk::UniqueDescriptorSetLayout, DSL_Num> m_DSL;
	std::array<vk::UniqueDescriptorSet, DS_Num> m_DS;

	std::array<vk::UniquePipeline, Pipeline_Num> m_pipeline;
	std::array<vk::UniquePipelineLayout, PipelineLayout_Num> m_PL;

	vk::UniqueRenderPass m_renderpass;
	vk::UniqueFramebuffer m_framebuffer;

	Context* m_ctx;
	ModelRenderer(Context& ctx, RenderTarget& rt)
	{
		m_ctx = &ctx;
//		auto cmd = ctx.m_ctx->m_cmd_pool->allocCmdTempolary(0);

		// descriptor set layout
// 		{
// 			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
// 			vk::DescriptorSetLayoutBinding binding[] = {
// 				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eStorageBuffer, 1, stage),
// 				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eUniformBuffer, 1, stage),
// 			};
// 			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
// 			desc_layout_info.setBindingCount(array_length(binding));
// 			desc_layout_info.setPBindings(binding);
// 			m_DSL[DSL_Renderer] = ctx.m_ctx->m_device.createDescriptorSetLayoutUnique(desc_layout_info);
// 		}

		// descriptor set
		{
// 			vk::DescriptorSetLayout layouts[] =
// 			{
// 				m_DSL[DSL_Renderer].get(),
// 			};
// 			vk::DescriptorSetAllocateInfo desc_info;
// 			desc_info.setDescriptorPool(ctx.m_ctx->m_descriptor_pool.get());
// 			desc_info.setDescriptorSetCount(array_length(layouts));
// 			desc_info.setPSetLayouts(layouts);
// 			m_DS[DS_Renderer] = std::move(ctx.m_ctx->m_device.allocateDescriptorSetsUnique(desc_info)[0]);
//
// 			vk::DescriptorBufferInfo uniforms[] =
// 			{
// 				u_info.getInfo(),
// 			};
// 			vk::DescriptorBufferInfo storages[] =
// 			{
// 				b_hashmap.getInfo(),
// 				b_hashmap_mask.getInfo(),
// 				b_interior_counter.getInfo(),
// 				b_leaf_counter.getInfo(),
// 				b_interior.getInfo(),
// 				b_leaf.getInfo(),
// 			};
// 
// 			vk::WriteDescriptorSet write[] =
// 			{
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eUniformBuffer)
// 				.setDescriptorCount(array_length(uniforms))
// 				.setPBufferInfo(uniforms)
// 				.setDstBinding(0)
// 				.setDstSet(m_DS[DS_Renderer].get()),
// 				vk::WriteDescriptorSet()
// 				.setDescriptorType(vk::DescriptorType::eStorageBuffer)
// 				.setDescriptorCount(array_length(storages))
// 				.setPBufferInfo(storages)
// 				.setDstBinding(1)
// 				.setDstSet(m_DS[DS_Renderer].get()),
// 			};
// 			ctx.m_ctx->m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);

		}
		// pipeline layout
		{
			{
				vk::DescriptorSetLayout layouts[] =
				{
//					m_DSL[DSL_Renderer].get(),
					sCameraManager::Order().getDescriptorSetLayout(sCameraManager::DESCRIPTOR_SET_LAYOUT_CAMERA),
					ctx.m_DSL[Context::DSL_Model_Material].get(),
					ctx.m_DSL[Context::DSL_Scene].get(),
										//					RenderTarget::s_descriptor_set_layout.get(),
				};
				vk::PipelineLayoutCreateInfo pipeline_layout_info;
				pipeline_layout_info.setSetLayoutCount(array_length(layouts));
				pipeline_layout_info.setPSetLayouts(layouts);
				m_PL[PipelineLayout_Render] = ctx.m_ctx->m_device.createPipelineLayoutUnique(pipeline_layout_info);
			}

		}

		// graphics pipeline render
		{

			// レンダーパス
			{
				// sub pass
				vk::AttachmentReference color_ref[] =
				{
					vk::AttachmentReference()
					.setAttachment(0)
					.setLayout(vk::ImageLayout::eColorAttachmentOptimal)
				};

				vk::SubpassDescription subpass;
				subpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
				subpass.setInputAttachmentCount(0);
				subpass.setPInputAttachments(nullptr);
				subpass.setColorAttachmentCount(array_length(color_ref));
				subpass.setPColorAttachments(color_ref);

				vk::AttachmentDescription attach_description[] =
				{
					// color1
					vk::AttachmentDescription()
					.setFormat(rt.m_info.format)
					.setSamples(vk::SampleCountFlagBits::e1)
					.setLoadOp(vk::AttachmentLoadOp::eLoad)
					.setStoreOp(vk::AttachmentStoreOp::eStore)
					.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal)
					.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal),
				};
				vk::RenderPassCreateInfo renderpass_info;
				renderpass_info.setAttachmentCount(array_length(attach_description));
				renderpass_info.setPAttachments(attach_description);
				renderpass_info.setSubpassCount(1);
				renderpass_info.setPSubpasses(&subpass);
				m_renderpass = ctx.m_ctx->m_device.createRenderPassUnique(renderpass_info);
			}

			{
				vk::ImageView view[] = 
				{
					rt.m_view,
				};
				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(m_renderpass.get());
				framebuffer_info.setAttachmentCount(array_length(view));
				framebuffer_info.setPAttachments(view);
				framebuffer_info.setWidth(rt.m_info.extent.width);
				framebuffer_info.setHeight(rt.m_info.extent.height);
				framebuffer_info.setLayers(1);
				m_framebuffer = ctx.m_ctx->m_device.createFramebufferUnique(framebuffer_info);
			}
			struct { const char* name; vk::ShaderStageFlagBits flag; } shader_param[] =
			{
				{"ModelRender.vert.spv", vk::ShaderStageFlagBits::eVertex},
				{"ModelRender.frag.spv", vk::ShaderStageFlagBits::eFragment},

			};
			std::array<vk::UniqueShaderModule, array_length(shader_param)> shader;
			std::array<vk::PipelineShaderStageCreateInfo, array_length(shader_param)> shaderStages;
			for (size_t i = 0; i < array_length(shader_param); i++)
			{
				shader[i] = loadShaderUnique(ctx.m_ctx->m_device, btr::getResourceShaderPath() + shader_param[i].name);
				shaderStages[i].setModule(shader[i].get()).setStage(shader_param[i].flag).setPName("main");
			}

			// assembly
			vk::PipelineInputAssemblyStateCreateInfo assembly_info;
			assembly_info.setPrimitiveRestartEnable(VK_FALSE);
			assembly_info.setTopology(vk::PrimitiveTopology::eTriangleList);

			// viewport
			vk::Viewport viewport[] =
			{
				vk::Viewport(0.f, 0.f, rt.m_resolution.width, rt.m_resolution.height, 0.f, 1.f),
			};
			vk::Rect2D scissor[] =
			{
				vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution),
			};
			vk::PipelineViewportStateCreateInfo viewportInfo;
			viewportInfo.setViewportCount(array_length(viewport));
			viewportInfo.setPViewports(viewport);
			viewportInfo.setScissorCount(array_length(scissor));
			viewportInfo.setPScissors(scissor);


			vk::PipelineRasterizationStateCreateInfo rasterization_info;
			rasterization_info.setPolygonMode(vk::PolygonMode::eFill);
			rasterization_info.setFrontFace(vk::FrontFace::eCounterClockwise);
			rasterization_info.setCullMode(vk::CullModeFlagBits::eNone);
			rasterization_info.setLineWidth(1.f);


			vk::PipelineMultisampleStateCreateInfo sample_info;
			sample_info.setRasterizationSamples(vk::SampleCountFlagBits::e1);

			vk::PipelineDepthStencilStateCreateInfo depth_stencil_info;
			depth_stencil_info.setDepthTestEnable(VK_TRUE);
			depth_stencil_info.setDepthWriteEnable(VK_TRUE);
			depth_stencil_info.setDepthCompareOp(vk::CompareOp::eGreaterOrEqual);
			depth_stencil_info.setDepthBoundsTestEnable(VK_FALSE);
			depth_stencil_info.setStencilTestEnable(VK_FALSE);


			std::vector<vk::PipelineColorBlendAttachmentState> blend_state = {
				vk::PipelineColorBlendAttachmentState()
				.setBlendEnable(VK_FALSE)
				.setColorWriteMask(vk::ColorComponentFlagBits::eR
					| vk::ColorComponentFlagBits::eG
					| vk::ColorComponentFlagBits::eB
					| vk::ColorComponentFlagBits::eA)
			};
			vk::PipelineColorBlendStateCreateInfo blend_info;
			blend_info.setAttachmentCount(blend_state.size());
			blend_info.setPAttachments(blend_state.data());

			vk::PipelineVertexInputStateCreateInfo vertex_input_info;
			vk::VertexInputBindingDescription vi_binding[] =
			{
				vk::VertexInputBindingDescription().setBinding(0).setStride(12).setInputRate(vk::VertexInputRate::eVertex),
				vk::VertexInputBindingDescription().setBinding(1).setStride(12).setInputRate(vk::VertexInputRate::eVertex),
				vk::VertexInputBindingDescription().setBinding(2).setStride(8).setInputRate(vk::VertexInputRate::eVertex),
			};
			vk::VertexInputAttributeDescription vi_attrib[] =
			{
				vk::VertexInputAttributeDescription().setLocation(0).setBinding(0).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
				vk::VertexInputAttributeDescription().setLocation(1).setBinding(1).setFormat(vk::Format::eR32G32B32Sfloat).setOffset(0),
				vk::VertexInputAttributeDescription().setLocation(2).setBinding(2).setFormat(vk::Format::eR32G32Sfloat).setOffset(0),
			};
			vertex_input_info.vertexBindingDescriptionCount = array_size(vi_binding);
			vertex_input_info.pVertexBindingDescriptions = vi_binding;
			vertex_input_info.vertexAttributeDescriptionCount = array_size(vi_attrib);
			vertex_input_info.pVertexAttributeDescriptions = vi_attrib;

			vk::GraphicsPipelineCreateInfo graphics_pipeline_info =
				vk::GraphicsPipelineCreateInfo()
				.setStageCount(shaderStages.size())
				.setPStages(shaderStages.data())
				.setPVertexInputState(&vertex_input_info)
				.setPInputAssemblyState(&assembly_info)
				.setPViewportState(&viewportInfo)
				.setPRasterizationState(&rasterization_info)
				.setPMultisampleState(&sample_info)
				.setLayout(m_PL[PipelineLayout_Render].get())
				.setRenderPass(m_renderpass.get())
				.setPDepthStencilState(&depth_stencil_info)
				.setPColorBlendState(&blend_info);
			m_pipeline[Pipeline_Render] = ctx.m_ctx->m_device.createGraphicsPipelineUnique(vk::PipelineCache(), graphics_pipeline_info).value;

		}

	}

	void execute_Render(vk::CommandBuffer cmd, RenderTarget& rt, Model& model)
	{
		DebugLabel _label(cmd, __FUNCTION__);
		{
			vk::ImageMemoryBarrier image_barrier[1];
			image_barrier[0].setImage(rt.m_image);
			image_barrier[0].setSubresourceRange(vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
			image_barrier[0].setOldLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
			image_barrier[0].setNewLayout(vk::ImageLayout::eColorAttachmentOptimal);
			image_barrier[0].setDstAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);

			cmd.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader | vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				{}, {}, { /*array_size(to_read), to_read*/ }, { array_size(image_barrier), image_barrier });
		}

		cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[Pipeline_Render].get());
//		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { m_DS[DSL_Renderer].get() }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 0, { sCameraManager::Order().getDescriptorSet(sCameraManager::DESCRIPTOR_SET_CAMERA) }, {});
		cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 2, { m_ctx->m_DS_Scene.get() }, {});

		vk::RenderPassBeginInfo begin_render_Info;
		begin_render_Info.setRenderPass(m_renderpass.get());
		begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), rt.m_resolution));
		begin_render_Info.setFramebuffer(m_framebuffer.get());
		cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

		auto& gltf_model = model.gltf_model;
		for (size_t mesh_index = 0; mesh_index < gltf_model.meshes.size(); ++mesh_index)
		{
			auto& gltf_mesh = gltf_model.meshes[mesh_index];

			for (size_t i = 0; i < gltf_mesh.primitives.size(); ++i)
			{
				tinygltf::Primitive primitive = gltf_mesh.primitives[i];
				for (auto& attrib : primitive.attributes)
				{
					tinygltf::Accessor accessor = gltf_model.accessors[attrib.second];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					int size = 1;
					if (accessor.type != TINYGLTF_TYPE_SCALAR)
					{
						size = accessor.type;
					}

					int vaa = -1;
					if (attrib.first.compare("POSITION") == 0) vaa = 0;
					if (attrib.first.compare("NORMAL") == 0) vaa = 1;
					if (attrib.first.compare("TEXCOORD_0") == 0) vaa = 2;
					if (vaa > -1)
					{
						cmd.bindVertexBuffers(vaa, model.b_vertex[bufferview.buffer].getInfo().buffer, model.b_vertex[bufferview.buffer].getInfo().offset + bufferview.byteOffset);
					}
					else
					{
						std::cout << "vaa missing: " << attrib.first << std::endl;
					}
				}

				cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_PL[PipelineLayout_Render].get(), 1, { model.m_DS_material[primitive.material].get() }, {});
				{
					tinygltf::Accessor accessor = gltf_model.accessors[primitive.indices];
					tinygltf::BufferView bufferview = gltf_model.bufferViews[accessor.bufferView];

					vk::IndexType indextype = vk::IndexType::eUint32;
					switch (accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: indextype = vk::IndexType::eUint32; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: indextype = vk::IndexType::eUint16; break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: indextype = vk::IndexType::eUint8EXT; break;
					default: assert(false); break;
					}
					cmd.bindIndexBuffer(model.b_vertex[bufferview.buffer].getInfo().buffer, model.b_vertex[bufferview.buffer].getInfo().offset + bufferview.byteOffset, indextype);

					cmd.drawIndexed(accessor.count, 1, 0, 0, 0);

				}
			}
		}


		cmd.endRenderPass();

	}
};



int main()
{

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(5.f, 5.f, 0.f);
	camera->getData().m_target = vec3(0.f, 0.1f, 0.f);
	camera->getData().m_up = vec3(0.f, -1.f, 0.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;
	auto ctx = std::make_shared<Context>(context);

	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);
	std::shared_ptr<Model> model = Model::LoadModel(*ctx, setup_cmd, btr::getResourceAppPath() + "pbr/DamagedHelmet.gltf");

	ModelRenderer renderer(*ctx, *app.m_window->getFrontBuffer());
	Skybox skybox(*ctx, setup_cmd, *app.m_window->getFrontBuffer());
//	Scene scene(*ctx, setup_cmd);

	app.setup();

	while (true)
	{
		cStopWatch time;

		app.preUpdate();
		{
			enum
			{
				cmd_render_clear,
				cmd_render,
				cmd_render_present,
				cmd_num
			};
			std::vector<vk::CommandBuffer> cmds(cmd_num);
			{
				cmds[cmd_render_clear] = clear_pipeline.execute();
				cmds[cmd_render_present] = present_pipeline.execute();
			}


			{
				auto cmd = context->m_cmd_pool->allocCmdOnetime(0);
				{
					ctx->execute(cmd);
					skybox.execute_Render(cmd, *ctx, *render_target);
					renderer.execute_Render(cmd, *render_target, *model);
					sAppImGui::Order().Render(cmd);
				}

				cmd.end();
				cmds[cmd_render] = cmd;

			}
			app.submit(std::move(cmds));
		}
		app.postUpdate();
		printf("%6.4fms\n", time.getElapsedTimeAsMilliSeconds());
	}

	return 0;
}

