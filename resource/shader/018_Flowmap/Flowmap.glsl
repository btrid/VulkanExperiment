#ifndef FLOWMAP_H_
#define FLOWMAP_H_

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
// #extension GL_KHR_shader_subgroup : require
#extension GL_KHR_shader_subgroup_ballot : require


#if defined(USE_Flowmap)

struct FlowmapInfo
{
	ivec2 reso;

};


layout(set=USE_Flowmap,binding=0, std140) uniform V0 {FlowmapInfo u_info; };
layout(set=USE_Flowmap,binding=1, scalar) buffer V1 { float b_value[]; };

#endif


#endif // FLOWMAP_H_