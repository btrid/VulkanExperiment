#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shading_language_include : require

#include </convertDimension.glsl>
#include </Shape.glsl>
#include </PM.glsl>
#include </Brick.glsl>

layout(location = 0)in vec3 inPosition;

out gl_PerVertex
{
	vec4 gl_Position;
};

out OutVertex{
	flat int Visible;
}Out;

uniform mat4 World = mat4(1.);
uniform mat4 PV = mat4(1.);

layout(std140, binding=0) uniform BrickParamUniform {
	BrickParam uParam;
};
layout(binding = 0, r32ui) uniform readonly uimage3D tBrickMap1;


void main()
{
/*	if(gl_InstanceID < uParam.num1.x*uParam.num1.y*uParam.num1.z)
	{
		ivec3 index = convert1DTo3D(int(gl_InstanceID), uParam.num1);
		uint i = imageLoad(tBrickMap1, index).x;
		Out.Visible = i != 0xFFFFFFFF ? 1 : 0;

		vec3 size = getCellSize1(uParam);
		gl_Position = PV * vec4((inPosition+vec3(index))*size+uParam.areaMin, 1.0);
	}else{
		Out.Visible = 1;
		gl_Position = PV * vec4(inPosition*(uParam.areaMax-uParam.areaMin), 1.0);
	}
*/
	if(gl_InstanceID < uParam.num0.x*uParam.num0.y*uParam.num0.z)
	{
		ivec3 index = convert1DTo3D(int(gl_InstanceID), uParam.num0);
		uint i = imageLoad(tBrickMap1, index).x;
		Out.Visible = i != 0x0 ? 1 : 0;

		vec3 size = getCellSize0(uParam);
		gl_Position = PV * vec4((inPosition+vec3(index))*size+uParam.areaMin, 1.0);
	}else{
		Out.Visible = 1;
		gl_Position = PV * vec4(inPosition*(uParam.areaMax-uParam.areaMin), 1.0);
	}

}
