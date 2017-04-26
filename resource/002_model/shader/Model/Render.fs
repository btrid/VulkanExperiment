#version 450

#extension GL_ARB_bindless_texture : require
#extension GL_NV_gpu_shader5 : require
#extension GL_ARB_shading_language_include : require

#include </MultiModel.glsl>
#include "C:/Users/tsuda/Source/Repos/GL/Resource/Shader/SVOGI/OctTree.sh"

in Vertex{
	flat int MaterialIndex;
	vec3 Position;
	vec3 Normal;
	vec3 Texcoord;
}FSIn;

layout(std140, binding=2) restrict buffer MaterialBuffer {
	Material materials[];
};

layout(location=0) out vec4 FragColor;
layout(location=1) out vec4 OutNormal;


vec4 LightPosition = vec4(10000.);
vec3 LightDiffuse = vec3(1.);
vec3 LightAmbient = vec3(0.5);
vec3 LightSpecular = vec3(0.5);

vec3 getColor()
{
	Material m = materials[FSIn.MaterialIndex];
	vec3 pos = FSIn.Position;
	vec3 norm = FSIn.Normal;
	vec3 s = normalize(LightPosition.xyz - pos);
	vec3 v = normalize(-pos.xyz);
	vec3 r = reflect( -s, norm );
	vec3 ambient = LightAmbient * (m.AmbientTex != 0 ? texture(sampler2D(m.AmbientTex), FSIn.Texcoord.xy).xyz : m.Ambient.xyz);
	float sDotN = max( dot(s,norm), 0.0 );
	vec3 diffuse = LightDiffuse * (m.DiffuseTex != 0? texture(sampler2D(m.DiffuseTex), FSIn.Texcoord.xy).xyz : m.Diffuse.xyz) * (sDotN + 0.5);
//	vec3 spec = vec3(0.0);
//	if( sDotN > 0.0 )
//		spec = LightSpecular * (m.SpecularTex != 0? texture(sampler2D(m.SpecularTex), FSIn.Texcoord.xy).xyz : m.Specular.xyz) * pow( max( dot(r,v), 0.0 ), m.Shininess);
	return ambient + diffuse /*+ spec*/;
}

void main()
{
//	int i = getMortonNumber(FSIn.Position.xyz, 0.);
//	color[i].rgba.rgb = getColor();
//	color[i].rgba.rgb = vec3(1.);
//	color[i].rgba.a = 1.;
	FragColor.xyz = getColor();
	FragColor.a = 1.;
}