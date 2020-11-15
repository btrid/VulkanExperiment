#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_DC 0
#include "DC.glsl"

layout(location=1) in Transform{
	vec3 Position;
}transform;

void main()
{

	uint a = uint(0);
	atomicOr(a, 1);	
//	imageAtomicOr(t_voxel_image[0], ivec3(0), 1);
//	imageAtomicOr(t_voxel_image[0], ivec3(0), 1);

}