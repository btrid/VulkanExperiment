#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_ARB_shading_language_include : require
#include </PMDefine.glsl>
#include </convertDimension.glsl>
#include </Shape.glsl>
#include </PM.glsl>
#include </Brick.glsl>

layout(binding=0, r32ui) uniform uimage3D tBrickMap0;
layout(binding=1, r32ui) uniform uimage3D tBrickMap1;

layout(std140, binding = 0) uniform BrickUniform
{
	BrickParam uParam;
};
layout(std430, binding=0) restrict buffer TriangleReferenceNumBuffer {
	uint bTriangleReferenceNum[];
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
	ivec3 indexMap1 = getIndexBrick1(uParam, transform.Position.xyz);
	if(isInRange(indexMap0, uParam.num0))
	{
		uint old0 = imageAtomicExchange(tBrickMap0, indexMap0, uint(1));

		uint tIndex = atomicCounterIncrement(aTriangleCount);
		uint old = imageAtomicExchange(tBrickMap1, getTiledIndexBrick1(uParam, indexMap1), tIndex);
		TriangleLL t;
		t.next = old;
		t.drawID = transform.DrawID;
		t.instanceID = transform.InstanceID;
		t.index[0] = transform.VertexID[0];
		t.index[1] = transform.VertexID[1];
		t.index[2] = transform.VertexID[2];

		uint mapIndex1D = convert3DTo1D(indexMap1, uParam.num1);
		uint triangleindex = atomicAdd(bTriangleReferenceNum[mapIndex1D], 1);
		bTriangleLL[mapIndex1D*TRIANGLE_BLOCK_NUM+triangleindex] = t;
	}


}