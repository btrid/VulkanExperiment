#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require
//#extension VK_KHR_shader_draw_parameters : require
layout(location = 0)in vec3 inPosition;

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out Vertex{
	vec3 Position;
	flat uint DrawID;
}Out;

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	Out.Position = gl_Position.xyz;
//	Out.DrawID = gl_DrawIndex;
	Out.DrawID = gl_DrawIDARB;

}
