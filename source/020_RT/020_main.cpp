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
#include <applib/DrawHelper.h>


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

#include <gli/gli/gli.hpp>

#define to_str(_a) #_a
#define naming(_a) {\
vk::DebugUtilsObjectNameInfoEXT name_info;\
name_info.objectHandle = reinterpret_cast<uint64_t&>(_a);\
name_info.objectType = vk::ObjectType::eImage;\
name_info.pObjectName = to_str(_a);\
ctx.m_device.setDebugUtilsObjectNameEXT(name_info);\
}

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


struct BRDFLookupTable
{
	AppImage m_brgf_lut;

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

		m_brgf_lut.m_view = ctx.m_device.createImageViewUnique(view_info);
		m_brgf_lut.m_sampler = ctx.m_device.createSamplerUnique(sampler_info);
		m_brgf_lut.m_image = std::move(image);
		m_brgf_lut.m_memory = std::move(image_memory);

		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.objectHandle = reinterpret_cast<uint64_t&>(m_brgf_lut.m_image.get());
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = to_str(m_brgf_lut.m_image.get());
		ctx.m_device.setDebugUtilsObjectNameEXT(name_info);

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
				.setLoadOp(vk::AttachmentLoadOp::eDontCare)
				.setStoreOp(vk::AttachmentStoreOp::eStore)
				.setInitialLayout(vk::ImageLayout::eUndefined)
				.setFinalLayout(vk::ImageLayout::eShaderReadOnlyOptimal),
			};
			vk::RenderPassCreateInfo renderpass_info;
			renderpass_info.setAttachmentCount(array_length(attach_description));
			renderpass_info.setPAttachments(attach_description);
			renderpass_info.setDependencies(dependencies);
			renderpass_info.setSubpassCount(1);
			renderpass_info.setPSubpasses(&subpass);


			auto m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);

			vk::ImageView view[] =
			{
				m_brgf_lut.m_view.get(),
			};

			vk::FramebufferCreateInfo framebuffer_info;
			framebuffer_info.setRenderPass(m_renderpass.get());
			framebuffer_info.setAttachmentCount(array_length(view));
			framebuffer_info.setPAttachments(view);
			framebuffer_info.setWidth(dim);
			framebuffer_info.setHeight(dim);
			framebuffer_info.setLayers(1);
//			framebuffer_info.setFlags(vk::FramebufferCreateFlagBits::eImageless);

// 			vk::FramebufferAttachmentImageInfo framebuffer_image_info;
// 			framebuffer_image_info.setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment);
// 			framebuffer_image_info.setLayerCount(1);
// 			framebuffer_image_info.setViewFormatCount(1);
// 			framebuffer_image_info.setPViewFormats(&format);
// 			framebuffer_image_info.setWidth(dim);
// 			framebuffer_image_info.setHeight(dim);
// 			vk::FramebufferAttachmentsCreateInfo framebuffer_attach_info;
// 			framebuffer_attach_info.setAttachmentImageInfoCount(1);
// 			framebuffer_attach_info.setPAttachmentImageInfos(&framebuffer_image_info);
// 
// 			framebuffer_info.setPNext(&framebuffer_attach_info);

			auto m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);

			// pipeline layout
			vk::PipelineLayoutCreateInfo pipeline_layout_info;
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
			vk::Rect2D scissor[] = { vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(dim, dim)) };
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
			begin_render_Info.setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(dim, dim)));
			begin_render_Info.setFramebuffer(m_framebuffer.get());

// 			vk::RenderPassAttachmentBeginInfo attachment_begin_info;
// 			std::array<vk::ImageView, 1> views = { m_brgf_lut.m_view.get() };
// 			attachment_begin_info.setAttachments(views);
// 
// 			begin_render_Info.pNext = &attachment_begin_info;

			cmd.beginRenderPass(begin_render_Info, vk::SubpassContents::eInline);

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
			cmd.draw(3, 1, 0, 0);
			cmd.endRenderPass();

			sDeleter::Order().enque(std::move(m_renderpass), std::move(m_framebuffer), std::move(PL), std::move(pipeline));
		}
	}
};

struct Environment
{
	enum Target { Target_IRRADIANCE = 0, Target_PREFILTEREDENV = 1, Target_Max, };
	struct PushBlockIrradiance
	{
		mat4 mvp;
		float deltaPhi = glm::two_pi<float>() / 180.0f;
		float deltaTheta = glm::half_pi<float>() / 64.0f;
	} pushBlockIrradiance;

	struct PushBlockPrefilterEnv
	{
		mat4 mvp;
		float roughness;
		uint32_t numSamples = 32u;
	} pushBlockPrefilterEnv;

	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS;
	vk::UniquePipelineLayout m_pipeline_layout;

	struct Offscreen
	{
		vk::ImageCreateInfo m_imageCI;
		vk::ImageViewCreateInfo m_vewCI;
		vk::UniqueImage m_image;
		vk::UniqueImageView m_view;
		vk::UniqueDeviceMemory m_memory;
		vk::UniqueRenderPass m_renderpass;
		vk::UniqueFramebuffer m_framebuffer;
	};

	std::array<Offscreen, Target_Max> m_offscreen;
	std::array<vk::UniquePipeline, Target_Max> m_pipeline;

	Environment(btr::Context& ctx, vk::CommandBuffer cmd)
	{
		for (uint32_t target = 0; target < Target_Max; target++)
		{
			int32_t dim = 512;
//			auto format = vk::Format::eR16G16B16A16Sfloat;
			auto format = vk::Format::eR32G32B32A32Sfloat;

			switch (target)
			{
			case Target_IRRADIANCE:
//				format = vk::Format::eR16G16B16A16Sfloat;
				dim = 64;
				break;
			case Target_PREFILTEREDENV:
//				format = vk::Format::eR16G16B16A16Sfloat;
				dim = 512;
				break;
			};
			auto& offscreen = m_offscreen[target];

			// Create target cubemap
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
				imageCI.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
				imageCI.flags = {};

				offscreen.m_image = ctx.m_device.createImageUnique(imageCI);

				vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(offscreen.m_image.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				offscreen.m_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
				ctx.m_device.bindImageMemory(offscreen.m_image.get(), offscreen.m_memory.get(), 0);

				// View
				vk::ImageViewCreateInfo viewCI{};
				viewCI.viewType = vk::ImageViewType::e2D;
				viewCI.format = format;
				viewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				viewCI.subresourceRange.levelCount = 1;
				viewCI.subresourceRange.layerCount = 1;
				viewCI.image = offscreen.m_image.get();
				offscreen.m_view = ctx.m_device.createImageViewUnique(viewCI);

				offscreen.m_imageCI = imageCI;
				offscreen.m_vewCI = viewCI;

				vk::ImageMemoryBarrier imageMemoryBarrier{};
				imageMemoryBarrier.image = offscreen.m_image.get();
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
				imageMemoryBarrier.srcAccessMask = {};
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
				imageMemoryBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });

				vk::DebugUtilsObjectNameInfoEXT name_info;
				name_info.objectHandle = reinterpret_cast<uint64_t&>(offscreen.m_image.get());
				name_info.objectType = vk::ObjectType::eImage;
				name_info.pObjectName = to_str(offscreen.m_image.get());
				ctx.m_device.setDebugUtilsObjectNameEXT(name_info);


			}

			// framebuffer
			{
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
					.setLoadOp(vk::AttachmentLoadOp::eDontCare)
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
				offscreen.m_renderpass = ctx.m_device.createRenderPassUnique(renderpass_info);

				vk::FramebufferCreateInfo framebuffer_info;
				framebuffer_info.setRenderPass(offscreen.m_renderpass.get());
				framebuffer_info.setAttachmentCount(1);
				framebuffer_info.setWidth(dim);
				framebuffer_info.setHeight(dim);
				framebuffer_info.setLayers(1);
				framebuffer_info.setFlags(vk::FramebufferCreateFlagBits::eImageless);

				vk::FramebufferAttachmentImageInfo framebuffer_image_info;
				framebuffer_image_info.setUsage(offscreen.m_imageCI.usage);
				framebuffer_image_info.setLayerCount(offscreen.m_imageCI.arrayLayers);
				framebuffer_image_info.setViewFormatCount(1);
				framebuffer_image_info.setPViewFormats(&format);
				framebuffer_image_info.setWidth(dim);
				framebuffer_image_info.setHeight(dim);
				framebuffer_image_info.setFlags(offscreen.m_imageCI.flags);
				vk::FramebufferAttachmentsCreateInfo framebuffer_attach_info;
				framebuffer_attach_info.setAttachmentImageInfoCount(1);
				framebuffer_attach_info.setPAttachmentImageInfos(&framebuffer_image_info);

				framebuffer_info.setPNext(&framebuffer_attach_info);

				offscreen.m_framebuffer = ctx.m_device.createFramebufferUnique(framebuffer_info);

			}


		}

		{
			// Descriptors
			vk::DescriptorSetLayoutBinding setLayoutBinding = vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment, nullptr };
			vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCI;
			descriptorSetLayoutCI.pBindings = &setLayoutBinding;
			descriptorSetLayoutCI.bindingCount = 1;
			m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(descriptorSetLayoutCI);

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

			// Pipeline layout
			vk::PushConstantRange pushConstantRange;
			pushConstantRange.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			pushConstantRange.size = glm::max(sizeof(PushBlockIrradiance), sizeof(PushBlockPrefilterEnv));

			vk::PipelineLayoutCreateInfo pipelineLayoutCI;
			pipelineLayoutCI.setLayoutCount = 1;
			pipelineLayoutCI.pSetLayouts = layouts;
			pipelineLayoutCI.pushConstantRangeCount = 1;
			pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;
			m_pipeline_layout = ctx.m_device.createPipelineLayoutUnique(pipelineLayoutCI);

			// Pipeline
			vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI;
			inputAssemblyStateCI.topology = vk::PrimitiveTopology::ePointList;

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

			std::array<vk::UniqueShaderModule, 4> shader;
			std::array<std::array<vk::PipelineShaderStageCreateInfo, 3>, Target_Max> shaderStages;
			shader[0] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "filtercube.vert.spv");
			shader[1] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "filtercube.geom.spv");
			shader[2] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "irradiancecube.frag.spv");
			shader[3] = loadShaderUnique(ctx.m_device, btr::getResourceShaderPath() + "prefilterenvmap.frag.spv");
			shaderStages[Target_IRRADIANCE][0].module = shader[0].get();
			shaderStages[Target_IRRADIANCE][0].stage = vk::ShaderStageFlagBits::eVertex;
			shaderStages[Target_IRRADIANCE][0].pName = "main";
			shaderStages[Target_IRRADIANCE][1].module = shader[1].get();
			shaderStages[Target_IRRADIANCE][1].stage = vk::ShaderStageFlagBits::eGeometry;
			shaderStages[Target_IRRADIANCE][1].pName = "main";
			shaderStages[Target_PREFILTEREDENV][0] = shaderStages[Target_IRRADIANCE][0];
			shaderStages[Target_PREFILTEREDENV][1] = shaderStages[Target_IRRADIANCE][1];
			shaderStages[Target_IRRADIANCE][2].module = shader[2].get();
			shaderStages[Target_IRRADIANCE][2].stage = vk::ShaderStageFlagBits::eFragment;
			shaderStages[Target_IRRADIANCE][2].pName = "main";
			shaderStages[Target_PREFILTEREDENV][2].module = shader[3].get();
			shaderStages[Target_PREFILTEREDENV][2].stage = vk::ShaderStageFlagBits::eFragment;
			shaderStages[Target_PREFILTEREDENV][2].pName = "main";

			vk::GraphicsPipelineCreateInfo pipelineCI;
			pipelineCI.layout = m_pipeline_layout.get();
			pipelineCI.renderPass = m_offscreen[Target_IRRADIANCE].m_renderpass.get();
			pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
			pipelineCI.pVertexInputState = &vertexInputStateCI;
			pipelineCI.pRasterizationState = &rasterizationStateCI;
			pipelineCI.pColorBlendState = &colorBlendStateCI;
			pipelineCI.pMultisampleState = &multisampleStateCI;
			pipelineCI.pViewportState = &viewportStateCI;
			pipelineCI.pDepthStencilState = &depthStencilStateCI;
			pipelineCI.pDynamicState = &dynamicStateCI;
			pipelineCI.stageCount = array_size(shaderStages[Target_IRRADIANCE]);
			pipelineCI.pStages = shaderStages[Target_IRRADIANCE].data();

			m_pipeline[Target_IRRADIANCE] = ctx.m_device.createGraphicsPipelineUnique({}, pipelineCI).value;

			pipelineCI.renderPass = m_offscreen[Target_PREFILTEREDENV].m_renderpass.get();
			pipelineCI.stageCount = array_size(shaderStages[Target_PREFILTEREDENV]);
			pipelineCI.pStages = shaderStages[Target_PREFILTEREDENV].data();
			m_pipeline[Target_PREFILTEREDENV] = ctx.m_device.createGraphicsPipelineUnique({}, pipelineCI).value;

		}

	}

	std::array<AppImage, Target_Max> execute(btr::Context& ctx, vk::CommandBuffer cmd, AppImage& env)
	{
		std::array<AppImage, Target_Max> cubemaps;
		vk::DescriptorImageInfo images[] = {
			env.info(),
		};
		vk::WriteDescriptorSet write[] =
		{
			vk::WriteDescriptorSet().setDstSet(*m_DS).setDstBinding(0).setDescriptorType(vk::DescriptorType::eCombinedImageSampler).setDescriptorCount(array_length(images)).setPImageInfo(images),
		};
		ctx.m_device.updateDescriptorSets(array_length(write), write, 0, nullptr);


		for (uint32_t target = 0; target < Target_Max; target++)
		{
			int32_t dim = 512;
			auto format = vk::Format::eR32G32B32A32Sfloat;
//			auto format = vk::Format::eR32G32B32A32Sfloat;

			switch (target)
			{
			case Target_IRRADIANCE:
//				format = vk::Format::eR16G16B16A16Sfloat;
				dim = 64;
				break;
			case Target_PREFILTEREDENV:
//				format = vk::Format::eR16G16B16A16Sfloat;
				dim = 512;
				break;
			};
			const uint32_t numMips = static_cast<uint32_t>(floor(log2(dim))) + 1;

			// Create target cubemap
			auto& cubemap = cubemaps[target];
			{
				// Image
				vk::ImageCreateInfo imageCI;
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
				cubemap.m_image = ctx.m_device.createImageUnique(imageCI);

				vk::MemoryRequirements memory_request = ctx.m_device.getImageMemoryRequirements(cubemap.m_image.get());
				vk::MemoryAllocateInfo memory_alloc_info;
				memory_alloc_info.allocationSize = memory_request.size;
				memory_alloc_info.memoryTypeIndex = Helper::getMemoryTypeIndex(ctx.m_physical_device, memory_request, vk::MemoryPropertyFlagBits::eDeviceLocal);

				cubemap.m_memory = ctx.m_device.allocateMemoryUnique(memory_alloc_info);
				ctx.m_device.bindImageMemory(cubemap.m_image.get(), cubemap.m_memory.get(), 0);

				// View
				vk::ImageViewCreateInfo viewCI{};
				viewCI.viewType = vk::ImageViewType::eCube;
				viewCI.format = format;
				viewCI.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
				viewCI.subresourceRange.levelCount = numMips;
				viewCI.subresourceRange.layerCount = 6;
				viewCI.image = cubemap.m_image.get();
				cubemap.m_view = ctx.m_device.createImageViewUnique(viewCI);

				// Sampler
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
				sampler_info.maxLod = numMips;
				sampler_info.maxAnisotropy = 0;
				sampler_info.anisotropyEnable = VK_FALSE;
				sampler_info.borderColor = vk::BorderColor::eFloatTransparentBlack;
				sampler_info.unnormalizedCoordinates = VK_FALSE;
				cubemap.m_sampler = ctx.m_device.createSamplerUnique(sampler_info);

				cubemap.m_imageCI = imageCI;
				cubemap.m_vewCI = viewCI;
			}


			auto& offscreen = m_offscreen[target];


			// Render cubemap
			vk::ClearValue clearValues[1];
			clearValues[0].color = vk::ClearColorValue{ std::array<float, 4>{0.0f, 0.0f, 0.2f, 0.0f} };

			vk::RenderPassBeginInfo renderPassBeginInfo;
			renderPassBeginInfo.renderPass = offscreen.m_renderpass.get();
			renderPassBeginInfo.framebuffer = offscreen.m_framebuffer.get();
			renderPassBeginInfo.renderArea.extent.width = offscreen.m_imageCI.extent.width;
			renderPassBeginInfo.renderArea.extent.height = offscreen.m_imageCI.extent.height;
			renderPassBeginInfo.clearValueCount = 1;
			renderPassBeginInfo.pClearValues = clearValues;

			vk::RenderPassAttachmentBeginInfo attachment_begin_info;
			std::array<vk::ImageView, 1> views = { offscreen.m_view.get() };
			attachment_begin_info.setAttachments(views);

			renderPassBeginInfo.pNext = &attachment_begin_info;

			std::vector<glm::mat4> matrices =
			{
				glm::lookAt(vec3(0.f), vec3(-1.f, 0.f, 0.f), vec3(0.f, -1.f, 0.f)),
				glm::lookAt(vec3(0.f), vec3( 1.f, 0.f, 0.f), vec3(0.f, -1.f, 0.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, -1.f, 0.f), vec3(0.f, 0.f, 1.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, 1.f, 0.f), vec3(0.f, 0.f, -1.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, 0.f, -1.f), vec3(0.f, -1.f, 0.f)),
				glm::lookAt(vec3(0.f), vec3(0.f, 0.f,  1.f), vec3(0.f, -1.f, 0.f)),
			};

			vk::Viewport viewport;
			viewport.width = (float)dim;
			viewport.height = (float)dim;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			vk::Rect2D scissor;
			scissor.extent.width = dim;
			scissor.extent.height = dim;

			vk::ImageSubresourceRange subresourceRange;
			subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
			subresourceRange.baseMipLevel = 0;
			subresourceRange.levelCount = numMips;
			subresourceRange.layerCount = 6;

			// Change image layout for all cubemap faces to transfer destination
			{
				vk::ImageMemoryBarrier imageMemoryBarrier;
				imageMemoryBarrier.image = cubemap.m_image.get();
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
				imageMemoryBarrier.srcAccessMask = {};
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
			}

			cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline[target].get());
			cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout.get(), 0, { m_DS.get() }, {});

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
					case Target_IRRADIANCE:
						pushBlockIrradiance.mvp = glm::ortho(-1.f, 1.f, -1.f, 1.f) * matrices[f];
						cmd.pushConstants(m_pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex|vk::ShaderStageFlagBits::eGeometry|vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlockIrradiance), & pushBlockIrradiance);
						break;
					case Target_PREFILTEREDENV:
						pushBlockPrefilterEnv.mvp = glm::ortho(-1.f, 1.f, -1.f, 1.f) * matrices[f];
						pushBlockPrefilterEnv.roughness = (float)m / (float)(numMips - 1);
						cmd.pushConstants(m_pipeline_layout.get(), vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushBlockPrefilterEnv), &pushBlockPrefilterEnv);
						break;
					};


					cmd.draw(1, 1, 0, 0);

					cmd.endRenderPass();

					{
						vk::ImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.image = offscreen.m_image.get();
						imageMemoryBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
						imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
						imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
						imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
						imageMemoryBarrier.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
						cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
					}

					// Copy region for transfer from framebuffer to cube face
					vk::ImageCopy copyRegion;

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

					cmd.copyImage(offscreen.m_image.get(), vk::ImageLayout::eTransferSrcOptimal, cubemap.m_image.get(), vk::ImageLayout::eTransferDstOptimal, { copyRegion });

					{
						vk::ImageMemoryBarrier imageMemoryBarrier;
						imageMemoryBarrier.image = offscreen.m_image.get();
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
				imageMemoryBarrier.image = cubemap.m_image.get();
				imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
				imageMemoryBarrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
				imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
				imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
				imageMemoryBarrier.subresourceRange = subresourceRange;
				cmd.pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {}, { imageMemoryBarrier });
			}
		}

		vk::DebugUtilsObjectNameInfoEXT name_info;
		name_info.objectHandle = reinterpret_cast<uint64_t&>(cubemaps[0].m_image.get());
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = to_str(cubemaps[0].m_image.get());
		ctx.m_device.setDebugUtilsObjectNameEXT(name_info);
		name_info.objectHandle = reinterpret_cast<uint64_t&>(cubemaps[1].m_image.get());
		name_info.objectType = vk::ObjectType::eImage;
		name_info.pObjectName = to_str(cubemaps[1].m_image.get());
		ctx.m_device.setDebugUtilsObjectNameEXT(name_info);
		return cubemaps;
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
		vec4 LightDir;

		float exposure;
		float gamma;

		int32_t skybox_render_type;
		float lod;

	};

	Environment m_env;

	RenderConfig m_render_config;
	btr::BufferMemoryEx<RenderConfig> u_render_config;

	BRDFLookupTable m_lut;
	AppImage m_environment;
	std::array<AppImage, 2> m_environment_filter;
	AppImage m_environment_debug;

	vk::UniqueDescriptorSet m_DS_Scene;

	Context(std::shared_ptr<btr::Context>& ctx, vk::CommandBuffer cmd)
		: m_env(*ctx, cmd)
		, m_lut(*ctx, cmd)
		
	{
		m_ctx = ctx;
		m_render_config.LightDir = vec4(1.f);
		m_render_config.exposure = 10.f;
		m_render_config.gamma = 2.2f;
		m_render_config.skybox_render_type = 0;
		m_render_config.lod = 0.f;
		u_render_config = ctx->m_uniform_memory.allocateMemory<RenderConfig>(1);
		// descriptor set layout
		{
			{
				auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
				vk::DescriptorSetLayoutBinding binding[] =
				{
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
					vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(11, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(12, vk::DescriptorType::eCombinedImageSampler, 1, stage),
					vk::DescriptorSetLayoutBinding(13, vk::DescriptorType::eCombinedImageSampler, 1, stage),
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
			m_environment.m_view = ctx->m_device.createImageViewUnique(view_info);
			m_environment.m_sampler = ctx->m_device.createSamplerUnique(sampler_info);

			m_environment_filter = m_env.execute(*ctx, cmd, m_environment);

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

			m_environment_debug.m_image = std::move(image);
			m_environment_debug.m_memory = std::move(image_memory);
			m_environment_debug.m_view = ctx->m_device.createImageViewUnique(view_info);
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

				vk::DescriptorImageInfo images[] = 
				{
					m_lut.m_brgf_lut.info(),
					m_environment.info(),
//					m_environment.info(),
//					m_environment.info(),
					m_environment_filter[0].info(),
					m_environment_filter[1].info(),
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
			ImGui::SetNextWindowSize(ImVec2(150.f, 300.f));
			ImGui::Begin("RenderConfig", &is_open, ImGuiWindowFlags_NoSavedSettings);
			ImGui::DragFloat4("Light Dir", &m_render_config.LightDir[0], 0.01f, -1.f, 1.f);
			ImGui::SliderFloat("exposure", &this->m_render_config.exposure, 0.0f, 20.f);

			ImGui::Separator();
			m_render_config.skybox_render_type;
			const char* types[] = { "normal", "irradiance", "prefiltered", };
			ImGui::Combo("Skybox", &m_render_config.skybox_render_type, types, array_size(types));
			ImGui::SliderFloat("lod", &m_render_config.lod, 0.f, 1.f);

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
			depth_stencil_info.setDepthWriteEnable(VK_FALSE);
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

#include <020_RT/Model.h>


int main()
{

	auto camera = cCamera::sCamera::Order().create();
	camera->getData().m_position = vec3(-0.2f, 3.1f, 0.02f);
	camera->getData().m_target = vec3(-0.2f, 1.1f, -0.03f);
	camera->getData().m_up = vec3(0.f, 0.f, 1.f);
	camera->getData().m_width = 1024;
	camera->getData().m_height = 1024;
	camera->getData().m_far = 5000.f;
	camera->getData().m_near = 0.01f;

	app::AppDescriptor app_desc;
	app_desc.m_window_size = uvec2(1024, 1024);
	app::App app(app_desc);

	auto context = app.m_context;

	auto setup_cmd = context->m_cmd_pool->allocCmdTempolary(0);
	auto ctx = std::make_shared<Context>(context, setup_cmd);


	auto render_target = app.m_window->getFrontBuffer();
	ClearPipeline clear_pipeline(context, render_target);
	PresentPipeline present_pipeline(context, render_target, context->m_window->getSwapchain());

	DrawHelper drawer{ context };

	std::shared_ptr<Model> model = Model::LoadModel(*ctx, setup_cmd, btr::getResourceAppPath() + "pbr/DamagedHelmet.gltf");

	ModelRenderer renderer(*ctx, *app.m_window->getFrontBuffer());
	Skybox skybox(*ctx, setup_cmd, *app.m_window->getFrontBuffer());

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
					drawer.draw(*context, cmd, *render_target, ctx->m_lut.m_brgf_lut);
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

