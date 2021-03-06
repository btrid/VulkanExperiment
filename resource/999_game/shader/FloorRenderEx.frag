#version 450
#pragma optionNV (unroll none)
#pragma optionNV (inline all)

//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#include <btrlib/Common.glsl>
#include <btrlib/Math.glsl>
#include <btrlib/Shape.glsl>

#define SETPOINT_CAMERA 0
#include <btrlib/Camera.glsl>

#define SETPOINT_MAP 1
#define SETPOINT_SCENE 2
#include </Map.glsl>

#define USE_LIGHT 3
#include <Light.glsl>

#include <Marching.glsl>

//layout(origin_upper_left) in vec4 gl_FragCoord;

layout(location=1) in FSIn{
	vec2 texcoord;
}In;

layout(location = 0) out vec4 FragColor;


void main() 
{
	MapDescriptor desc = u_map_info.m_descriptor[0];

	vec3 foward = normalize(u_camera[0].u_target - u_camera[0].u_eye).xyz;
	vec3 side = cross(foward, u_camera[0].u_up.xyz);
	side = dot(side, side) <= 0.1 ? vec3(1., 0., 0.) : normalize(side);
	vec3 up = u_camera[0].u_up.xyz;

	// イメージセンサー上の位置
	float tan_fov_y = tan(u_camera[0].u_fov_y / 2.);

	vec3 position_on_sensor =
		(u_camera[0].u_eye.xyz + foward)
		+ side * -In.texcoord.x * tan_fov_y * u_camera[0].u_aspect
		+ up * In.texcoord.y * tan_fov_y;
	// レイを飛ばす方向。
	vec3 dir = normalize(position_on_sensor - u_camera[0].u_eye.xyz);
	vec3 pos = u_camera[0].u_eye.xyz;

	Ray ray = MakeRay(pos, dir);
	vec2 area = desc.m_cell_num * desc.m_cell_size;
	vec3 bmin = vec3(0) - 0.1;
	vec3 bmax = vec3(area.x, WALL_HEIGHT*5, area.y) + 0.1;
	Hit hit = marchToAABB(ray, bmin, bmax);

	if(hit.IsHit == 0)
	{
		discard;
	}

	pos = hit.HitPoint;
	ivec3 map_index = calcMapIndex(desc, pos);
	for(;;)
	{
		ivec3 next = marchEx3D(pos, map_index, dir);
		if(any(lessThan(map_index.xz, ivec2(0)))
		|| any(greaterThanEqual(map_index.xz, ivec2(desc.m_cell_num))))
		{
			// Mapの外に出た
			discard;
		}
		uint map = imageLoad(t_map, map_index.xz).x;
		vec3 p = pos + next *1.;
		if(next.y != 0)
		{
			// 地面と当たった場合
			if(p.y <= map*WALL_HEIGHT)
			{
				FragColor = vec4(vec3(map*1., 0., 1.) * calcEmission(pos), 1.);
				break;
			}
		}
		else
		{
			if(p.y <= map*WALL_HEIGHT)
			{
				// 壁と当たった場合
				FragColor = vec4(vec3(1., 0., 0.) * calcEmission(pos), 1.);
				break;
			}
		}
	}


	vec4 p = u_camera[0].u_projection * u_camera[0].u_view * vec4(pos, 1.);
	p /= p.w;
	gl_FragDepth = p.z;

	vec2 tex01 = (p.xy * 0.5) + 0.5;
	tex01 = 1.-tex01;
	uvec2 tile_index = uvec2(tex01 * u_tile_info.m_tile_num);
	uint tile_index_1d = tile_index.x + tile_index.y * u_tile_info.m_tile_num.x;
	uint tile_map_index_offset = tile_index_1d * u_tile_info.m_tile_index_map_max;
	uint light_num = b_tile_data_counter[tile_index_1d];
	
	FragColor = vec4(0.);
	for(uint i = 0; i < light_num; i++)
	{
		FragColor += b_light[b_tile_data_map[tile_map_index_offset+i]].m_emissive / light_num;
	}

}
