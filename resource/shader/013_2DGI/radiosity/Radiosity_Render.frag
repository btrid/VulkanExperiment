#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"


layout(location=1)in Data
{
	flat u16vec2 center;
	vec3 color;
}fs_in;

layout(location = 0) out f16vec4 FragColor;
void main()
{
	vec2 target = vec2(gl_FragCoord.xy);
	FragColor = f16vec4(fs_in.color*10. * fs_in.color / (distance(target, vec2(fs_in.center))+1.), 0.01);
}

