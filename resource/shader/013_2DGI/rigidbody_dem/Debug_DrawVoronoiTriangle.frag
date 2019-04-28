#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Rigidbody2D 0
#include "Rigidbody2D.glsl"

layout(location = 0) out vec4 FragColor;

void main()
{

	FragColor = vec4(0.2, 0.2, 1., 1.);

}