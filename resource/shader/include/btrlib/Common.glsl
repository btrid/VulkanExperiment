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



#endif