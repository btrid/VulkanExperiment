#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require
//#include </Math.glsl>
#include </Light.glsl>

layout(location = 0)in vec3 inPosition;

out gl_PerVertex{
	vec4 gl_Position;
};

struct Vertex
{
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
};
layout(location = 0) out Vertex VSOut;

void main()
{
	LightParam light = b_light[gl_instanceID];
	vec4 pos = vec4((inPosition + light.m_position).xyz, 1.0);
	gl_Position = uProjection * uView * vec4(pos.xyz, 1.0);

}
