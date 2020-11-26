#ifndef LDC_H_
#define LDC_H_

#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require

struct Info
{
	vec4 m_aabb_min;
	vec4 m_aabb_max;
	uint m_vertex_num;
};
struct LDCPoint
{
	float p;
	uint normal;
	uint inout_next;
};
struct LDCCell
{
	uint usepoint_xyz;
	uvec3 normal;
};

//layout(set=0,binding=0) uniform accelerationStructureEXT topLevelAS;
layout(set=1,binding=0, std140) uniform InfoUniform {Info u_info; };
layout(set=1,binding=1, scalar) buffer Vertices { vec3 b_vertex[]; };
layout(set=1,binding=2, scalar) buffer Normals { vec3 b_normal[]; };
layout(set=1,binding=3, scalar) buffer Indices { uvec3 b_index[]; };
layout(set=1,binding=10, std430) buffer LDCCounter { int b_ldc_counter; };
layout(set=1,binding=11, std430) buffer LDCPointLinkHead { int b_ldc_point_link_head[]; };
layout(set=1,binding=12, scalar) buffer LDCPointBuffer { LDCPoint b_ldc_point[]; };
layout(set=1,binding=13, scalar) buffer LDCCellBuffer { LDCCell b_ldc_cell[]; };
layout(set=1,binding=14, scalar) buffer DCVVertex { vec3 b_dcv_vertex[]; };


// https://discourse.panda3d.org/t/glsl-octahedral-normal-packing/15233
f16vec2 sign_not_zero(in f16vec2 v) 
{
    return fma(step(f16vec2(0.0), v), f16vec2(2.0), f16vec2(-1.0));
}
f16vec2 pack_normal_octahedron(in vec3 _v)
{
//	v.xy /= dot(abs(v), vec3(1));
	f16vec3 v = f16vec3(_v.xy / dot(abs(_v), vec3(1)), _v.z);
	return mix(v.xy, (float16_t(1.0) - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, float16_t(0.0)));

}
vec3 unpack_normal_octahedron(in f16vec2 packed_nrm)
{

	f16vec3 v = f16vec3(packed_nrm.xy, float16_t(1.0) - abs(packed_nrm.x) - abs(packed_nrm.y));
	v.xy = mix(v.xy, (float16_t(1.0) - abs(v.yx)) * sign_not_zero(v.xy), step(v.z, float16_t(0)));
	return normalize(v);
}
#endif // LDC_H_

