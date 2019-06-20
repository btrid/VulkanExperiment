#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Data
{
    flat vec3 color;
	flat uint vertex_index;
} vs_out;

void main()
{
    vs_out.color = b_vertex_array[gl_InstanceIndex].radiance[(Bounce_Num%2)];
//    vs_out.color = b_vertex_array[gl_InstanceIndex].radiance[0];
    vs_out.vertex_index = gl_InstanceIndex;
    gl_Position = vec4(1.);
}
