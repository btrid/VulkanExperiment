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
	flat i16vec4 voronoi_minmax;
}fs_in;

void main()
{
	uvec2 coord = uvec2(gl_FragCoord.xy);
	uint f_index = coord.x + coord.y * u_gi2d_info.m_resolution.x;

	if(!isDiffuse(b_fragment[f_index])){ return; }

	// 削除
	setDiffuse(b_fragment[f_index], false);
	vec3 rgb = getRGB(b_fragment[f_index]);
	setRGB(b_fragment[f_index], vec3(1.));

	// 重心
	uvec2 p_pos = coord.xy - fs_in.voronoi_minmax.xy;
	atomicAdd(b_make_rigidbody.cm_work.x, int64_t(p_pos.x*CM_WORK_PRECISION));
	atomicAdd(b_make_rigidbody.cm_work.y, int64_t(p_pos.y*CM_WORK_PRECISION));

	// particle生成
	uint p_index = atomicAdd(b_make_rigidbody.pnum, 1);
	if((p_index % RB_PARTICLE_BLOCK_SIZE) == 0)
	{
		atomicAdd(b_make_param.pb_num.x, 1);
	}
	b_make_particle[p_index].flag = RBP_FLAG_ACTIVE;
	b_make_particle[p_index].pos = vec2(coord);
	b_make_particle[p_index].pos_old = vec2(coord);
	b_make_particle[p_index].color = packUnorm4x8(vec4(rgb, 1.));

	// sdf用
	uvec2 reso = fs_in.voronoi_minmax.zw - fs_in.voronoi_minmax.xy;
	b_make_jfa_cell[p_pos.x + p_pos.y * reso.x] = i16vec2(0xfffe);

}