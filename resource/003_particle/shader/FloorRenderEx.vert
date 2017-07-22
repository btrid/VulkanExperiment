#version 450

#pragma optionNV (unroll all)
#pragma optionNV (inline all)

#extension GL_ARB_shader_draw_parameters : require
//#extension GL_ARB_bindless_texture : require


#ifdef VULKAN
#extension GL_GOOGLE_cpp_style_line_directive : require
//#extension GL_KHR_vulkan_glsl : require
#else
#extension GL_ARB_shading_language_include : require
#endif


layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};
layout(location=1) out VSOut{
	vec2 texcoord;
}Out;

void main()
{
	vec2 pos = vec2(gl_VertexIndex/2, gl_VertexIndex%2) * 2. - 1.;
	gl_Position = vec4(pos, 0., 1.);
	Out.texcoord = gl_Position.xy;
}
