#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require

#define USE_Rigidbody2D 0
#define USE_MakeRigidbody 2
#define USE_GI2D 1
#include "GI2D.glsl"
#include "Rigidbody2D.glsl"


layout(location=1)in FSIn
{
	flat i16vec4 minmax;
}fs_in;

void main()
{
	int index = int(gl_FragCoord.x + gl_FragCoord.y * u_gi2d_info.m_resolution.x);
	uvec2 p_index = uvec2(gl_FragCoord.xy) - fs_in.minmax.xy;
	uvec2 reso = fs_in.minmax.zw - fs_in.minmax.xy;

	// 削除
//	vec3 rgb = getRGB(b_fragment[index]);
	setRGB(b_fragment[index], vec3(0., 0., 1.));
	vec3 rgb = vec3(0., 0., 1.);
	setDiffuse(b_fragment[index], false);

	// 重心
	atomicAdd(b_make_rigidbody.cm_work.x, int64_t(p_index.x*CM_WORK_PRECISION));
	atomicAdd(b_make_rigidbody.cm_work.y, int64_t(p_index.y*CM_WORK_PRECISION));

	// particle生成
	uint index = atomicAdd(b_make_rigidbody.pnum, 1);
	b_make_particle[index].flag = RBP_FLAG_ACTIVE;
	b_make_particle[index].pos = vec2(gl_FragCoord.xy);
	b_make_particle[index].pos_old = vec2(gl_FragCoord.xy);
	b_make_particle[index].color = packUnorm4x8(vec4(rgb, 1.));

	// sdf用
	b_make_jfa_cell[p_index.x + p_index.y * reso.x] = i16vec2(0xfffe);

}