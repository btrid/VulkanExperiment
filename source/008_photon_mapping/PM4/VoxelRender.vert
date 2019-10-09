#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require
#extension GL_NV_shader_atomic_float : require
#include </convertDimension.glsl>
#include </convert_vec4_to_int.glsl>
#include </Shape.glsl>
#include </PMDefine.glsl>
#include </Voxel.glsl>


layout(location = 0)in vec3 inPosition;

out gl_PerVertex
{
	vec4 gl_Position;
};

out out_vertex{
	vec4 albedo;
}Out;

uniform mat4 World = mat4(1.);
uniform mat4 PV = mat4(1.);
uniform int uHierarchy;

layout(binding=0, r32f) uniform readonly image3D tVoxelAlbedoR;
layout(binding=1, r32f) uniform readonly image3D tVoxelAlbedoG;
layout(binding=2, r32f) uniform readonly image3D tVoxelAlbedoB;
layout(binding=3, r32ui) uniform readonly uimage3D tVoxelAlbedoCount;


void main()
{
	if(gl_InstanceID < OctreeCellNum[uHierarchy])
	{
		ivec3 index = convert1DTo3D(int(gl_InstanceID), ivec3(OctreeCellSide[uHierarchy]));
//		ivec3 index = convert1DTo3D(int(gl_InstanceID), ivec3(BRICK_SUB_SIZE));
		uint count = imageLoad(tVoxelAlbedoCount, index).r;
		if(count != 0)
		{
			vec3 albedo = vec3(	imageLoad(tVoxelAlbedoR, index).r,
								imageLoad(tVoxelAlbedoG, index).r,
								imageLoad(tVoxelAlbedoB, index).r);
			Out.albedo = vec4(albedo / float(count), 1.);
		}
		else
		{
			Out.albedo = vec4(0.);
		}

		vec3 size = getVoxelCellSize()*float(1 << (VOXEL_DEPTH-uHierarchy) );
		gl_Position = PV * vec4((inPosition+vec3(index))*size+BRICK_AREA_MIN + size*0.5, 1.0);
	}else{
		Out.albedo = vec4(1.);
		gl_Position = PV * vec4(inPosition*(BRICK_AREA_MAX-BRICK_AREA_MIN), 1.0);
	}

}
