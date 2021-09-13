#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_AppModel 0
#define USE_AppModel_Render 1
#include "applib/AppModel.glsl"


layout(early_fragment_tests) in;

layout(location = 1) in Vertex 
{
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
	flat int DrawID;
}FSIn;

layout(location=0) out vec4 FragColor;


vec4 LightPosition = vec4(10000.);
vec3 LightDiffuse = vec3(1.);
vec3 LightAmbient = vec3(0.5);
vec3 LightSpecular = vec3(0.5);

vec3 getColor()
{
	uint material_index = 0;
	vec3 pos = FSIn.Position;
	vec3 norm = FSIn.Normal;
	vec3 s = normalize(LightPosition.xyz - pos);
	vec3 v = normalize(-pos.xyz);
	vec3 r = reflect( -s, norm );
//	vec3 ambient = LightAmbient * (m.AmbientTex != 0 ? texture(sampler2D(m.AmbientTex), FSIn.Texcoord.xy).xyz : m.Ambient.xyz);
	float sDotN = max( dot(s,norm), 0.0 );
//	vec3 diffuse = LightDiffuse * (m.DiffuseTex != 0? texture(sampler2D(m.DiffuseTex), FSIn.Texcoord.xy).xyz : m.Diffuse.xyz) * (sDotN + 0.5);
	vec3 diffuse = /*LightDiffuse * */texture(t_albedo_texture[material_index], FSIn.Texcoord.xy).xyz;
	return /*ambient +*/ diffuse /*+ spec*/;

//	return diffuse;
}

void main()
{
//	int i = getMortonNumber(FSIn.Position.xyz, 0.);
//	color[i].rgba.rgb = getColor();
//	color[i].rgba.rgb = vec3(1.);
//	color[i].rgba.a = 1.;
	FragColor.rgb = getColor();
	FragColor.a = 1.;
}

