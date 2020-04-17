#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_KHR_shader_subgroup_ballot: enable

layout(push_constant) uniform Input
{
	mat4 pvw;
} constant;


layout(location = 0)in vec3 in_pos;


layout(location=0) out gl_PerVertex{
	vec4 gl_Position;
};

void main()
{
	gl_Position =  constant.pvw * vec4(in_pos, 1.);

}