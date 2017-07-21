#version 450
#pragma optionNV (unroll none)
#pragma optionNV (inline all)

//#extension GL_KHR_vulkan_glsl : require
#extension GL_GOOGLE_cpp_style_line_directive : require
#extension GL_ARB_shader_image_load_store : require

#include </Shape.glsl>
//#include </Marching.glsl>

#define SETPOINT_CAMERA 0
#include <Common.glsl>

#define SETPOINT_MAP 1
#include </Map.glsl>

#define WALL_HEIGHT (3.)

layout(location=1) in FSIn{
	vec2 texcoord;
}In;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out float OutZ;


vec3 getCellSize3D()
{
	return vec3(u_map_info.cell_size.x, 5., u_map_info.cell_size.y);
}
vec3 clampByMapIndex(in vec3 pos, in ivec3 map_index)
{
	vec3 cell_size = getCellSize3D();
	float particle_size = 0.;
	return clamp(pos, (vec3(map_index) * cell_size)+particle_size+FLT_EPSIRON, (map_index+ivec3(1)) * cell_size-particle_size-FLT_EPSIRON);
}
ivec3 calcMapIndex(in vec3 p)
{
	vec3 cell_size = vec3(u_map_info.cell_size.x, 5., u_map_info.cell_size.y);
	ivec3 map_index = ivec3(p.xyz / cell_size);
	return map_index;
}
bool marchCB(inout vec3 pos, inout ivec3 map_index, inout vec3 dir, in vec3 next_pos, in ivec3 next_map_index)
{
	map_index = next_map_index;
	pos = next_pos;
	
	return true;
}
ivec3 marchEx(inout vec3 pos, inout ivec3 map_index, in vec3 dir)
{

	MapInfo map_info = u_map_info;
	vec3 cell_size = vec3(map_info.cell_size.x, 5., map_info.cell_size.y);
	float particle_size = 0.;
	vec3 cell_origin = vec3(map_index)*cell_size;
	vec3 cell_p = pos - cell_origin;
	float x = dir.x < 0. ? cell_p.x : (cell_size.x- cell_p.x);
	float y = dir.y < 0. ? cell_p.y : (cell_size.y- cell_p.y);
	float z = dir.z < 0. ? cell_p.z : (cell_size.z- cell_p.z);
	x = (x <= particle_size ? cell_size.x + x : x) - particle_size;
	y = (y <= particle_size ? cell_size.y + y : y) - particle_size;
	z = (z <= particle_size ? cell_size.z + z : z) - particle_size;

	vec3 dist = vec3(9999.);
	dist.x = abs(dir.x) < FLT_EPSIRON ? 9999.9 : abs(x / dir.x);
	dist.y = abs(dir.y) < FLT_EPSIRON ? 9999.9 : abs(y / dir.y);
	dist.z = abs(dir.z) < FLT_EPSIRON ? 9999.9 : abs(z / dir.z);
	float rate = min(dist.x, min(dist.y, dist.z));

	vec3 prog = dir * rate;
	ivec3 next = ivec3(0);
	if(dist.x < dist.y && dist.x < dist.z){
		next.x = dir.x < 0. ? -1 : 1;
	}
	else if(dist.y < dist.z)
	{
		next.y = dir.y < 0. ? -1 : 1;
	}
	else
	{
		next.z = dir.z < 0. ? -1 : 1;
	}
	vec3 next_pos = pos + prog + vec3(next)*FLT_EPSIRON;
	pos = next_pos;
	map_index += next;
	return next;
}


void main() 
{
	vec2 resolution = vec2(640., 480.);
	vec3 foward = normalize(u_target - u_eye).xyz;
	vec3 side = cross(foward, vec3(0., 1., 0.));
	side = dot(side, side) <= 0.1 ? vec3(-1., 0., 0.) : normalize(side);
	vec3 up = normalize(cross(side, foward));

	vec2 r = vec2(0.5);
	// イメージセンサー上の位置。
	vec3 position_on_sensor =
		(u_eye.xyz + foward)
		+ side * (float(r.x + In.texcoord.x) / resolution.x - 0.5)*2.
		+ up * (float(r.y + In.texcoord.y) / resolution.y - 0.5)*2.;
	// レイを飛ばす方向。
	vec3 dir = normalize(position_on_sensor - u_eye.xyz);
	vec3 pos = normalize(position_on_sensor);
	ivec3 map_index = calcMapIndex(position_on_sensor);
	for(;;)
	{
		ivec3 next = marchEx(pos, map_index, dir);
		uint map = imageLoad(t_map, map_index.xz).x;
		if(next.y != 0)
		{
			// 地面と当たった場合
			FragColor = vec4(0., 0.3, 0.9, 1.);
			break;
		}else{
			if(pos.y <= 5.*map)
			{
				FragColor = vec4(0.7, 0.4, 0.4, 1.);
				break;
			}
		}
	}
	OutZ = 1.;

}
