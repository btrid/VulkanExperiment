#ifndef CULLING_GLSL_
#define CULLING_GLSL_

#include <btrlib/Common.glsl>

#ifdef USE_CULLING
layout(std430, set=USE_CULLING, binding=0) buffer DrawParameterBuffer {
	mat4 b_world[];
};
layout(std430, set=USE_CULLING, binding=1) restrict buffer VisibleBuffer {
	uint b_visible[];
};

layout(std430, set=USE_CULLING, binding=2) buffer DrawCommandBuffer {
	DrawElementIndirectCommand b_cmd;
};
layout(std430, set=USE_CULLING, binding=3) buffer MappingBuffer {
	uint b_instance_index_map[];
};
#endif

#endif