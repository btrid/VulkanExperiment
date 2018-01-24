#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

layout(location = 0)in ivec2 inPosition;

layout(location = 0) out gl_PerVertex{
	vec4 gl_Position;
};


void main()
{
	gl_Position = vec4(inPosition / 1000., 1., 1.);
}
