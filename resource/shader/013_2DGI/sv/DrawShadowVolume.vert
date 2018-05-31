#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/Math.glsl"

#define USE_SV 0
#include "SV.glsl"

layout(location = 0)in vec2 in_position;

out gl_PerVertex{
	vec4 gl_Position;
};

/*layout(location=1) out ModelData
{
	vec2 texcoord;
}out_modeldata;
*/
void main()
{
	gl_Position = u_sv_info.m_camera_PV * vec4(in_position.x, 0., in_position.y, 1.);
}
