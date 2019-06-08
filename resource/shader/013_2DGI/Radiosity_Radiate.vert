#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Data
{
	flat uint index;
	vec4 color;
} vs_out;

void main()
{
    gl_Position = vec4(0, 0, 0., 1.);

	vs_out.index = gl_InstanceIndex;
	u16vec2 pos = b_vertex_array[vs_out.index].pos;
	vs_out.color = unpackRGB(b_light[getMemoryOrder(pos)]).xyzz;

}
