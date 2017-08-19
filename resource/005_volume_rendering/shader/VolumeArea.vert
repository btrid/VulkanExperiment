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

//layout(location = 0)in vec3 inPosition;

out gl_PerVertex
{
	vec4 gl_Position;
};

out OutVertex{
	flat ivec3 index;
}Out;

uniform mat4 World = mat4(1.);
uniform mat4 PV = mat4(1.);


void main()
{
	Out.index = convert1DTo3D(int(gl_InstanceID), ivec3(128));
	gl_Position = PV * vec4((vec3(Out.index)), 1.0);

}
