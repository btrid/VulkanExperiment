#version 460
#extension GL_GOOGLE_include_directive : require


#define USE_GI2D 0
#include "GI2D.glsl"

#define USE_AppModel 1
#define USE_AppModel_Render 2
#include "AppModel.glsl"


layout(location=1) in ModelData
{
	vec2 texcoord;
}in_modeldata;

void setAlbedo(in vec3 albedo)
{
	ivec2 vpos = ivec2(gl_FragCoord.xy);
	int index1D = int(vpos.x + vpos.y * u_gi2d_info.m_resolution.x);
	b_fragment[index1D] = makeFragment(albedo, true, false);
}

void main()
{
	vec3 diffuse = texture(t_albedo_texture[0], in_modeldata.texcoord.xy).xyz;
	setAlbedo(diffuse);
}