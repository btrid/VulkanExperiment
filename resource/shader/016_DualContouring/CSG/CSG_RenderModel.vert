#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_Model 0
#include "CSG.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"


layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out VSVertex
{
	vec3 normal;
	flat uvec3 CellID;
}Out;


layout(push_constant) uniform pushConstants 
{
	mat4 world;
} constants;

void main()
{
	int face = gl_VertexIndex/3;
	int component = gl_VertexIndex%3;
	uint index = b_index[face][component];
	gl_Position = u_camera[0].u_projection * u_camera[0].u_view * constants.world * vec4(b_vertex[index], 1.);
	Out.normal =  (constants.world * vec4(b_normal[index], 1.)).xyz;

}
