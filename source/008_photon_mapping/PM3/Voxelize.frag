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


layout(binding=0, r8ui) uniform writeonly uimage3D tBrickMap0;

layout(std140, binding = 0) uniform BrickUniform
{
	BrickParam uParam;
};
layout(std430, binding = 0) restrict buffer TriangleLLHeadBuffer{
	uint bTriangleLLHead[];
};
layout(std430, binding=1) writeonly restrict buffer TriangleLLBuffer {
	TriangleLL bTriangleLL[];
};

layout(binding = 0) uniform atomic_uint  aTriangleCount;

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
	if(isInRange(indexMap0, uParam.num0))
	{
		imageStore(tBrickMap0, indexMap0, ivec4(1));
		uint tIndex = atomicCounterIncrement(aTriangleCount);

		ivec3 indexMap1 = getIndexBrick1(uParam, transform.Position.xyz);
		uint old = atomicExchange(bTriangleLLHead[getTiledIndexBrick1(uParam, indexMap1)], tIndex);
		TriangleLL t;
		t.next = old;
		t.drawID = transform.DrawID;
		t.instanceID = transform.InstanceID;
		t.index[0] = transform.VertexID[0];
		t.index[1] = transform.VertexID[1];
		t.index[2] = transform.VertexID[2];

		bTriangleLL[tIndex] = t;
	}
}