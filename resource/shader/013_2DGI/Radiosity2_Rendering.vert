#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity2 1
#include "GI2D.glsl"
#include "Radiosity2.glsl"


layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location=1) out OUT
{
	vec2 texcoord;
}vs_out;
void main()
{
    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    vs_out.texcoord.x = (x+1.0)*0.5;
    vs_out.texcoord.y = (y+1.0)*0.5;
    gl_Position = vec4(x, y, 0, 1);
}
