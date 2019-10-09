#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in int inMaterialIndex;
layout(location = 3)in vec2 inTexcoord0;
layout(location = 4)in vec2 inTexcoord1;

out gl_PerVertex
{
	vec4 gl_Position;
};

out Vertex{
	vec3 Position;
	vec3 Normal;
	vec4 Texcoord; //[u,v][u,v]
	flat int MaterialIndex;
}Out;

uniform mat4 World = mat4(1.);

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	Out.Position = gl_Position.xyz;
	Out.Normal = inNormal;
	Out.Texcoord = vec4(inTexcoord0, inTexcoord1);
	Out.MaterialIndex = inMaterialIndex;
}
