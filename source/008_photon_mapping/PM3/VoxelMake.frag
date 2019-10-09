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
#include </convertDimension.glsl>
#include </Shape.glsl>
#include </PMDefine.glsl>
#include </Brick.glsl>
#include </PM.glsl>


layout(binding=0, r32ui) uniform uimage3D tBrickMap0;

layout(std140, binding = 0) uniform BrickUniform
{
	BrickParam uParam;
};

layout(binding = 0) uniform atomic_uint  aBrick0Count;

in Transform{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

layout(location = 0) out vec4 FragColor;

void main()
{
	
	ivec3 indexMap0 = getIndexBrick0(uParam, transform.Position.xyz);
	if(!isInRange(indexMap0, uParam.num0))
	{ return; }
	uint index = imageAtomicCompSwap(tBrickMap0, indexMap0, 0xFFFFFFFF, 0);
	if(index != 0xFFFFFFFF)
	{ return; }
	
	uint newIndex = atomicCounterIncrement(aBrick0Count);
	imageStore(tBrickMap0, indexMap0, ivec4(newIndex));
}