#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_GI2D_RENDER 1
#include "GI2D.glsl"

out gl_PerVertex{
	vec4 gl_Position;
};

void main()
{
    vec2 xy = vec2((gl_VertexIndex.xx & ivec2(1, 2)) << ivec2(2, 1));
	gl_Position = vec4(xy * 2. - 1., 0, 1);
}
