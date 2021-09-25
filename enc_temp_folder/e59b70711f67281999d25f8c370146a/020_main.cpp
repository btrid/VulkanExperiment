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

	RenderConfig m_render_config;
	btr::BufferMemoryEx<RenderConfig> u_render_config;
	Image m_environment;

	vk::UniqueDescriptorSet m_DS_Scene;

	Context(std::shared_ptr<btr::Context>& ctx)
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
			sampler_info.maxLod = 0.f;
			sampler_info.maxAnisotropy = 1.0;
			sampler_info.anisotropyEnable = VK_FALSE;
			sampler_info.borderColor = vk::BorderColor::eFloatOpaqueWhite;

			m_environment.m_image = std::move(image);
			m_environment.m_memory = std::move(image_memory);
			m_environment.m_image_view = ctx->m_device.createImageViewUnique(view_info);
			m_environment.m_sampler = ctx->m_device.createSamplerUnique(sampler_info);

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
				ImGui::SliderFloat("exposure", &this->m_render_config.exposure, 0.001f, 1000.f);
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
				m_renderpass = ctx.m_ctx->m_device.createRenderPassUnique(renderpass_info);
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
//					renderer.execute_Render(cmd, *render_target, *model);
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

