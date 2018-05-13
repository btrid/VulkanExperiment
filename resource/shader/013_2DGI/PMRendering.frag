#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require

#include <btrlib/ConvertDimension.glsl>
#include <btrlib/Shape.glsl>

#define USE_PM 0
#define USE_PM_RENDER 1
#include <PM.glsl>


layout(location = 0) out vec4 FragColor;
void main()
{
//	ivec2 index = ivec2(in_modeldata.texcoord * u_pm_info.m_resolution.xy);
//	FragColor = b_color[index.x + index.y * int(u_pm_info.m_resolution.x)]*10.;
	vec3 rgb = vec3(0.);
	uvec2 reso = textureSize(s_color[0], 0).xy;
//	uvec2 reso = u_pm_info.m_resolution.xy;
	vec2 uv = gl_FragCoord.xy/reso;
	rgb.rgb += texture(s_color[0], uv).rgb;
	rgb.rgb += texture(s_color[1], uv).rgb;
	rgb.rgb += texture(s_color[2], uv).rgb;
	FragColor = vec4(rgb, 1.);

}