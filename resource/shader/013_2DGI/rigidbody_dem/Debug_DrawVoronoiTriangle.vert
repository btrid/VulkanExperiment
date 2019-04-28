#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Rigidbody2D 0
#include "Rigidbody2D.glsl"


layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	gl_Position = vec4(1.0);
}
