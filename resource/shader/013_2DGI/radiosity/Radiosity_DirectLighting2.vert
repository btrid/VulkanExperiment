#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(location = 0)in ivec2 in_pos;
layout(location = 1)in vec4 in_color;
layout(location = 2)in ivec2 in_target;


layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out Vertex{
	flat i16vec2 pos;
	flat vec3 color;
}vs_out;

void main()
{
	vs_out.pos = i16vec2(in_pos);
	vs_out.color = vec3(in_color.xyz);

	gl_Position = vec4(in_target / vec2(u_gi2d_info.m_resolution.xy) * 2. - 1., 0., 1.);

}