#include <applib/AppModel/AppModel.h>
#include <applib/AppModel/AppModelPipeline.h>
#include <applib/cModelPipeline.h>
#include <applib/DrawHelper.h>
#include <applib/sCameraManager.h>

DescriptorSet<AppModelAnimateDescriptor::Set> createAnimateDescriptorSet(const std::shared_ptr<AppModel>& appModel)
{
	AppModelAnimateDescriptor::Set descriptor_set;
	descriptor_set.m_model_info = appModel->m_model_info_buffer.getInfoEx();
	descriptor_set.m_instancing_info = appModel->m_instancing_info_buffer.getInfoEx();
	descriptor_set.m_node_info = appModel->m_node_info_buffer.getInfoEx();
	descriptor_set.m_bone_info = appModel->m_bone_info_buffer.getInfoEx();
	descriptor_set.m_animation_info = appModel->m_animationinfo_buffer.getInfoEx();
	descriptor_set.m_playing_animation = appModel->m_animationplay_buffer.getInfoEx();
	descriptor_set.m_anime_indirect = appModel->m_animation_skinning_indirect_buffer.getInfoEx();
	descriptor_set.m_node_transform = appModel->m_node_transform_buffer.getInfoEx();
	descriptor_set.m_bone_transform = appModel->m_bone_transform_buffer.getInfoEx();
	descriptor_set.m_instance_map = appModel->m_instance_map_buffer.getInfoEx();
	descriptor_set.m_draw_indirect = appModel->m_draw_indirect_buffer.getInfoEx();
	descriptor_set.m_world = appModel->m_world_buffer.getInfoEx();
	descriptor_set.m_motion_texture[0].imageView = appModel->m_motion_texture[0].getImageView();
	descriptor_set.m_motion_texture[0].sampler = appModel->m_motion_texture[0].getSampler();
	descriptor_set.m_motion_texture[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	return sModelAnimateDescriptor::Order().allocateDescriptorSet(std::move(descriptor_set));
}

DescriptorSet<AppModelRenderDescriptor::Set> createRenderDescriptorSet(const std::shared_ptr<AppModel>& appModel)
{
	AppModelRenderDescriptor::Set descriptor_set;
	descriptor_set.m_bonetransform = appModel->m_bone_transform_buffer.getInfoEx();
	descriptor_set.m_model_info = appModel->m_model_info_buffer.getInfoEx();
	descriptor_set.m_instancing_info = appModel->m_instancing_info_buffer.getInfoEx();
	descriptor_set.m_material = appModel->m_material.getInfoEx();
	descriptor_set.m_material_index = appModel->m_material_index.getInfoEx();
	descriptor_set.m_images.fill(vk::DescriptorImageInfo(sGraphicsResource::Order().getWhiteTexture().m_sampler.get(), sGraphicsResource::Order().getWhiteTexture().m_image_view.get(), vk::ImageLayout::eShaderReadOnlyOptimal));
	for (size_t i = 0; i < appModel->m_texture.size(); i++)
	{
		const auto& tex = appModel->m_texture[i];
		if (tex.isReady()) {
			descriptor_set.m_images[i] = vk::DescriptorImageInfo(tex.getSampler(), tex.getImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
		}
	}
	return sModelRenderDescriptor::Order().allocateDescriptorSet(std::move(descriptor_set));
}
