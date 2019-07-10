#version 450
#extension GL_GOOGLE_include_directive : require

#define USE_GI2D 0
#define USE_GI2D_Radiosity 1
#include "GI2D.glsl"
#include "Radiosity.glsl"


layout(location=1)in Data
{
	vec2 texcoord;
}fs_in;

layout(location = 0) out f16vec4 FragColor;
void main()
{
	for(int i = 0; i < 1; i++)
	{
		FragColor = f16vec4(FragColor + texture(s_radiosity[i], fs_in.texcoord)*0.14);
	}
//	uvec2 index = uvec2(gl_FragCoord.xy);
//	bool is_diffuse = isDiffuse(b_fragment[index.x + index.y * u_gi2d_info.m_resolution.x]);
//	if(is_diffuse) discard;
//	FragColor = f16vec4(fs_in.color*0.14, 0.0);
}
