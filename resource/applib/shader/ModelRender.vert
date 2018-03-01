#version 450

//#extension GL_ARB_shader_draw_parameters : require

#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/Math.glsl>

#define USE_MODEL_INFO_SET 0
#include <applib/model/MultiModel.glsl>

#define SETPOINT_CAMERA 1
#include <btrlib/Camera.glsl>

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in vec4 inTexcoord;
layout(location = 3)in uvec4 inBoneID;
layout(location = 4)in vec4 inWeight;


out gl_PerVertex{
	vec4 gl_Position;
};

struct Vertex
{
	vec2 Texcoord;
};
layout(location = 0) out Vertex VSOut;

void main()
{
	float x = float((gl_VertexID & 1) << 2);
    float y = float((gl_VertexID & 2) << 1);
	VSOut.Texcoord = vec2(x, y);
	gl_Position = vec4(x-1., y-1., 0, 1);
 }
