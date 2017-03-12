#version 330

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

//in Vertex{
//	vec4 Color;
//}FSIn;

layout(location=0) out vec4 FragColor;
//layout(location=1) out vec4 OutNormal;

in FSIn{
	vec4 color;
}In;

void main()
{
	FragColor.xyz = vec3(1.0);
	FragColor.a = 1.;
//	FragColor = In.color;
}

