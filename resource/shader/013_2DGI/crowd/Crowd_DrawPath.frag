#version 460
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_Crowd2D 0
#include "Crowd.glsl"

#define USE_SYSTEM 1
#include "applib/System.glsl"

#define USE_GI2D 2
#include "GI2D.glsl"

layout(push_constant) uniform Input
{
	ivec2 render_size;
} constant;

layout (local_size_x=32, local_size_y=32) in;
void main()
{
	uvec2 index = gl_GlobalInvocationID.xy;
	

}
