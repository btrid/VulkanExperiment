#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Data
{
	flat uint vertex_index;
} vs_out;

void main()
{
    vs_out.vertex_index = gl_InstanceIndex;
    gl_Position = vec4(1.);
}
