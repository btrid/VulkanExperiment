#version 450

#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_draw_parameters : require
//#extension GL_KHR_vulkan_glsl : require

layout(location = 0)in vec3 inPosition;

out gl_PerVertex
{
	vec4 gl_Position;
};

out Vertex{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID;
}Out;

void main()
{
	gl_Position = vec4(0.5, 0.5, 0.5, 1.0);
	Out.Position = inPosition.xyz;
//	Out.DrawID = gl_DrawIDARB;
	Out.DrawID = 0;
	Out.InstanceID = gl_InstanceIndex;
	Out.VertexID = gl_VertexIndex;
}
