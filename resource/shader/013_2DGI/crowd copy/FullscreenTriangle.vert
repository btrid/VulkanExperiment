#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_RT 1
#include "GI2D/GI2D.glsl"

out gl_PerVertex{
	vec4 gl_Position;
};
layout(location=1) out OUT
{
	vec2 texcoord;
}vs_out;

void main()
{
    vec2 xy = -1. - vec2((gl_VertexIndex.xx & ivec2(1, 2)) << ivec2(2, 1));
	vs_out.texcoord = (xy+1.) * 0.5;
	gl_Position = vec4(xy, 0, 1);

//  float x = -1.0 + float((gl_VertexIndex & 1) << 2);
//  float y = -1.0 + float((gl_VertexIndex & 2) << 1);
//  vs_out.texcoord.x = (x+1.0)*0.5;
//  vs_out.texcoord.y = (y+1.0)*0.5;
//  gl_Position = vec4(x, y, 0, 1);
}
