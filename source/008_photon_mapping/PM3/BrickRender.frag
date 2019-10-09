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

#include </convertDimension.glsl>
#include </Shape.glsl>
#include </PMDefine.glsl>
#include </Brick.glsl>
#include </PM.glsl>

in Transform{
	flat int Visible;
}FSIn;

//layout(early_fragment_tests) in;
//layout(post_depth_coverage) in;
layout(location = 0) out vec4 FragColor;

void main()
{	
#if 0
	FragColor = FSIn.Visible != 0 ? vec4(1.) : vec4(0.2);
#else
	if(FSIn.Visible == 0){
		discard;
		return;
	}

	FragColor = vec4(1.);

#endif

}