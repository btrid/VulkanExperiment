#version 460

#extension GL_GOOGLE_include_directive : require

#define USE_AS 0
#define USE_LDC 1
#include "LDC.glsl"

layout(location = 0) rayPayloadInEXT float RayMaxT;
//layout(location = 1) rayPayloadInEXT LDCPoint Point;

hitAttributeEXT vec2 baryCoord;
void main()
{
  RayMaxT = gl_RayTmaxEXT;

	uvec3 i0 = b_index[gl_PrimitiveID];
	vec3 n0 = b_normal[i0.x];
	vec3 n1 = b_normal[i0.y];
	vec3 n2 = b_normal[i0.z];

  LDCPoint point;
  point.p = gl_HitTEXT;

	const vec3 barycentric = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);
	vec3 Normal = normalize(n0 * barycentric.x + n1 * barycentric.y + n2 * barycentric.z);
  point.normal = pack_normal_octahedron(Normal);

	int index = atomicAdd(b_ldc_counter, 1);
  uint grid = gl_LaunchIDEXT.x + gl_LaunchIDEXT.y*gl_LaunchSizeEXT.x + gl_LaunchIDEXT.z*gl_LaunchSizeEXT.x*gl_LaunchSizeEXT.y;
 	int head = b_ldc_point_link_head[grid];
  b_ldc_point_link_head[grid] = index;
  
  point.inout_next = /*(gl_HitKindEXT==gl_HitKindFrontFacingTriangleEXT?0:(1<<31)) |*/ head;
  b_ldc_point[index] = point;



}
