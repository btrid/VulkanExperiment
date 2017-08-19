#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)
#pragma optionNV(fastmath on)
//#pragma optionNV(fastprecision on)
//#pragma optionNV(ifcvt all)
#pragma optionNV(strict on)
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require

layout(binding = 0)uniform sampler3D uTex;


in Vertex{
	flat ivec3 index;
}FSIn;

layout(location = 0) out vec4 FragColor;

void main()
{	
#if 0
	FragColor = vec4(1.);
#else
	FragColor = vec4(vec3(1.), texture(uTex, vec3(FSIn.index) / vec3(128.)).r);

#endif

}