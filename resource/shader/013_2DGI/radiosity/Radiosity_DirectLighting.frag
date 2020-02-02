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
layout(location = 0) out vec4 FragColor;
void main()
{
	float dist = distance(vec2(fs_in.pos.xy), vec2(gl_FragCoord.xy));
	vec3 color = fs_in.color/(1.+dist);

	if(dot(color, vec3(1.)) < 0.01)
	{
		color = vec3(1., 0., 0.);
		//discard;
	}

	FragColor = f16vec4(color, 1.0);



//	チェック用
	FragColor.xyz = dot(FragColor.xyz, vec3(1.)) <= 0. ? vec3(1., 0., 0.) : vec3(1.); 
}
