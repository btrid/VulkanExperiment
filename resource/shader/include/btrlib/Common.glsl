#ifndef BTRLIB_COMMON_GLSL
#define BTRLIB_COMMON_GLSL


struct DrawIndirectCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

struct DrawElementIndirectCommand{
	uint  count;
	uint  instanceCount;
	uint  firstIndex;
	uint  baseVertex;
	uint  baseInstance;
};

bool isInAABB2D(in vec2 p, in vec4 aabb)
{
	return all(lessThan(p, aabb.zw)) && all(greaterThan(p, aabb.xy));
}
bool isContainAABB2D(in vec2 p, in float scale, in vec4 aabb)
{
	return all(lessThan(p-scale, aabb.zw)) && all(greaterThan(p+scale, aabb.xy));
}
#endif