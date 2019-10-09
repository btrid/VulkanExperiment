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

out VSOUT{
	flat uint IndexID;
}VsOut;


void main()
{
	gl_Position = vec4(inPosition.xyz, 1.);
}
