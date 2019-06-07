#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"

out gl_PerVertex{
	vec4 gl_Position;
};

void main()
{
    gl_Position = vec4(x, y, 0., 1.);
}
