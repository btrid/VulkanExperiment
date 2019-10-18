#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"

layout(push_constant) uniform Constant
{
	vec4 pos;
	vec4 color;
} constant;

/*layout(location=1)in Data
{
	flat vec3 color;
}fs_in;
*/
layout(location = 0) out f16vec4 FragColor;
void main()
{
//	FragColor = f16vec4(vec3(5.), 1.0);

//	vec2 pos = ;
	float dist = distance(constant.pos.xy, vec2(gl_FragCoord.xy));
	FragColor = f16vec4(constant.color.xyz/(dist*0.01), 1.0);
}
