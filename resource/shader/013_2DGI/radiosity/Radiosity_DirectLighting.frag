#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D/GI2D.glsl"
#include "GI2D/Radiosity.glsl"


layout(location=1) in Vertex{
	flat i16vec2 pos;
	flat vec3 color;
}fs_in;
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
	float dist = distance(vec2(fs_in.pos.xy), vec2(gl_FragCoord.xy));
	vec3 color = fs_in.color/(1.+dist*dist*0.01)-0.01;
	color = dot(color, vec3(1.)) <= 0. ? vec3(1., 0., 0.) : vec3(1.); // チェック用
	FragColor = f16vec4(color, 1.0);
}
