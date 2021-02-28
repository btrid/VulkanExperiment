#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_Voxel 0
#define USE_Model 1
#include "Voxel.glsl"

layout(location = 0)in vec3 inPosition;

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location=1) out Vertex{
	vec3 Position;
	vec3 Albedo;
}Out;

void main()
{
	gl_Position = vec4(inPosition, 1.0);
	Out.Position = gl_Position.xyz;

//	uint material_index = b_voxel_model_mesh_info[gl_DrawID].material_index;
//	vec3 albedo = b_voxel_model_material[material_index].albedo.xyz;
	
	Out.Albedo = vec3(0.);

}
