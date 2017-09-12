#version 450

#ifdef VULKAN
#extension GL_GOOGLE_cpp_style_line_directive : require
#else
#extension GL_ARB_shading_language_include : require
#endif

layout(location = 0)in vec3 inPosition;
layout(location = 1)in vec3 inNormal;
layout(location = 2)in vec3 inAlbedo;
layout(location = 3)in vec3 inEmission;

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out Vertex{
	vec3 Position;
}Out;

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	Out.Position = gl_Position.xyz;
}
