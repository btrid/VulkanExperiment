#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LayeredDepth 0
#include "CSG.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};


void main()
{
//    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
//    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
//    gl_Position = vec4(x, y, 1, 1);

    vec2 outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0f + -1.0f, 0.0f, 1.0f);
}
