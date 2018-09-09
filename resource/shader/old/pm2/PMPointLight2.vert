#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/Math.glsl>

#define USE_PM 0
#include <PM2.glsl>

out gl_PerVertex{
	vec4 gl_Position;
	float gl_PointSize;
};

layout(push_constant) uniform InputVertex
{
	vec2 pos;
} constant;

void main()
{
//	vec4 pos = vec4((inPosition).xyz, 1.0);
//	pos = u_OIT_info.m_camera_PV * skinningMat * pos;

//	vec4 pos = vec4(300., 300., 0., 1.0);
	vec2 pos01 = constant.pos.xy / u_pm_info.m_resolution.xy;
	vec2 pos11 = pos01 * 2. - 1.;
	gl_Position = vec4(pos11, 0., 1.0);
	gl_PointSize = 10.;
}
