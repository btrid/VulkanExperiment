#version 460

#extension GL_GOOGLE_include_directive : require

#define USE_DC 0
#define USE_Model 1
#include "DualContouring.glsl"


layout(push_constant) uniform pushConstants 
{
	vec4 axis[3];
	vec4 pos;
} constants;

layout (local_size_x = 8, local_size_y = 8) in;


struct LDCCtx
{
	vec3 f;
	vec3 s;
	vec3 u;
	mat3 rotate;
	float t;
	float tmax;
	vec3 origin;

};

void init()
{
	LDCCtx ldc_ctx;
	ldc_ctx.f = constants.axis[(gl_GlobalInvocationID.z+0)%3].xyz;
	ldc_ctx.s = constants.axis[(gl_GlobalInvocationID.z+1)%3].xyz; 
	ldc_ctx.u = constants.axis[(gl_GlobalInvocationID.z+2)%3].xyz;

	ldc_ctx.rotate = transpose(mat3(constants.axis[0].xyz, constants.axis[1].xyz, constants.axis[2].xyz));

	vec3 extent = Voxel_Block_Size;

	ldc_ctx.t = 0.;
	ldc_ctx.tmax = extent[gl_GlobalInvocationID.z];

	vec3 rate = vec3(gl_GlobalInvocationID) / vec3(Voxel_Reso);
	ldc_ctx.origin = (s*rate.x*extent[(gl_GlobalInvocationID.z+1)%3]+u*rate.y*extent[(gl_GlobalInvocationID.z+2)%3]);

	vec3 pos = constants.pos.xyz;
	ldc_ctx.origin -= f*pos[(gl_GlobalInvocationID.z+0)%3] + (s*pos[(gl_GlobalInvocationID.z+1)%3]+u*pos[(gl_GlobalInvocationID.z+2)%3]);

	return ldc_ctx;
}

LDCPoint getLDCPoint(inout LDCCtx ldc_ctx)
{
	rayQueryEXT rayQuery;
	rayQueryInitializeEXT(rayQuery,	topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, origin, t, f, tmax);
	while(rayQueryProceedEXT(rayQuery)) {}

	t = rayQueryGetIntersectionTEXT(rayQuery, true);
	if(t >= tmax)
	{ 
		return g_invalid_point;
	}

	uvec3 i0 = b_index[rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true)];
	vec3 a = b_vertex[i0.x];
	vec3 b = b_vertex[i0.y];
	vec3 c = b_vertex[i0.z];
	vec3 normal = normalize(cross(b - a, c - a));

	float voxel_p = (t/Voxel_Size)[gl_GlobalInvocationID.z];

	LDCPoint point;
	point.p = u8vec2(floor(voxel_p), int(fract(voxel_p)*253.+1.));
	point.normal = pack_normal_octahedron(g_rotate * normal);
	point.flag = rayQueryGetIntersectionFrontFaceEXT(rayQuery, true) ? LDCFlag_Incident : LDCFlag_Exit;
	return point;

}

void main() 
{
	// loop
	{
		LDCPoint point_b = getLDCPoint(t, tmax, origin, f);
		if(!is_valid(point_b)){ return; }

		int grid = int(gl_GlobalInvocationID.x + gl_GlobalInvocationID.y*Voxel_Reso.x + gl_GlobalInvocationID.z*Voxel_Reso.x*Voxel_Reso.y);
		int index_a = b_ldc_point_link_head[grid];

		LDCPoint point_a = g_invalid_point;
		if(index_a>=0) 
		{
			point_a = b_ldc_point[index_a];
			int next_a = b_ldc_point_link_next[index_a];
			free_ldc_point(index_a);
			index_a = next_a;
		}

		bool is_incident_a = false;
		bool is_incident_b = false;
		int head = -1;

		if((point_b.flag & LDCFlag_Exit) == LDCFlag_Exit)
		{
			LDCPoint point = {u8vec2(0, 1), LDCFlag_Incident, pack_normal_octahedron(-f)};
			int index = allocate_ldc_point();
			if(head==-1) { b_ldc_point_link_head[grid] = index; }
			else { b_ldc_point_link_next[head] = index; }
			b_ldc_point[index] = point;

			head = index;
			is_incident_b = true;
		}

		for(;is_valid(point_a)||is_valid(point_b);)
		{
			bool is_a = false;
			bool is_b = false;
			LDCPoint point = g_invalid_point;
			if(point_a.p[0]<=point_b.p[0])
			{
				if(!is_incident_a && !is_incident_b)
				{
					// 開始
					point = point_a;
				}
				else if(is_incident_a && !is_incident_b)
				{
					// 終了
					point = point_a;
				}

				is_a = true;
				is_incident_a = !is_incident_a;

			}
			if(point_a.p[0]>=point_b.p[0])
			{
				if(!is_incident_a && !is_incident_b)
				{
					// 開始
					point = point_b;
				}
				else if(!is_incident_a && is_incident_b)
				{
					// 終了
					point = point_b;
				}

				is_b = true;
				is_incident_b = !is_incident_b;

			}
			if(is_a)
			{
				if(index_a>=0)
				{
					point_a = b_ldc_point[index_a];
					int next = b_ldc_point_link_next[index_a];
					free_ldc_point(index_a);
					index_a = next;
				}
				else
				{
					point_a = g_invalid_point;
				}
			}
			if(is_b)
			{
				point_b = getLDCPoint(t, tmax, origin, f);
			}

			if(is_valid(point))
			{
				int index = allocate_ldc_point();
				if(head==-1) { b_ldc_point_link_head[grid] = index; }
				else { b_ldc_point_link_next[head] = index; }
				b_ldc_point[index] = point;
				head = index;
			}
		}

		if(is_incident_b)
		{
			LDCPoint point = {u8vec2(255, 254), LDCFlag_Exit, pack_normal_octahedron(f)};
			int index = allocate_ldc_point();
			if(head==-1) { b_ldc_point_link_head[grid] = index; }
			else { b_ldc_point_link_next[head] = index; }
			b_ldc_point[index] = point;

			head = index;
			is_incident_b = false;
		}

		if(head==-1) { b_ldc_point_link_head[grid] = -1; }
		else { b_ldc_point_link_next[head] = -1; }

	}

}