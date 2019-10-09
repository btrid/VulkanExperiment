#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;

out gl_PerVertex
{
	vec4 gl_Position;
};

out Vertex{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID;
}Out;

uniform mat4 World = mat4(1.);

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	Out.Position = gl_Position.xyz;
	Out.DrawID = gl_DrawIDARB;
	Out.InstanceID = gl_InstanceID;
	Out.VertexID = gl_VertexID;
}
