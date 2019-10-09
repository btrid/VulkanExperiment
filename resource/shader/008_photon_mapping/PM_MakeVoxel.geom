#version 460
#extension GL_GOOGLE_include_directive : require
#extension VK_EXT_conservative_rasterization : require
#define USE_PM 0
#include "PM.glsl"


layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;


in gl_PerVertex
{
	vec4 gl_Position;
//	float gl_PointSize;
//	float gl_ClipDistance[];
} gl_in[];

layout(location=1) in InVertex{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID;
}In[];

layout(location=0) out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

layout(location=1) out Transform{
	vec3 Position;
	flat int DrawID;
	flat int InstanceID;
	flat int VertexID[3];
}transform;

vec4 toQuat(vec3 eular)
{
	vec3 c = cos(eular * 0.5);
	vec3 s = sin(eular * 0.5);

	vec4 q;
	q.w = c.x * c.y * c.z + s.x * s.y * s.z;
	q.x = s.x * c.y * c.z - c.x * s.y * s.z;
	q.y = c.x * s.y * c.z + s.x * c.y * s.z;
	q.z = c.x * c.y * s.z - s.x * s.y * c.z;		
	return q;
}

vec3 rotate(vec4 q, vec3 v)
{
	vec3 QuatVector = q.xyz;
	vec3 uv = cross(QuatVector, v);
	vec3 uuv = cross(QuatVector, uv);
	uv *= (2. * q.w);
	uuv *= 2.;

	return v + uv + uuv;
}

mat4 orthoLH01(float l, float r, float b, float t, float n, float f)
{
	mat4 o = mat4(1.);
	o[0][0] = 2. / (r - l);
	o[1][1] = 2. / (t - b);
	o[2][2] = 2. / (f - n);
	o[3][0] = - (r + l) / (r - l);
	o[3][1] = - (t + b) / (t - b);
	o[3][2] = - n / (f - n);
	return o;
}

mat4 orthoVec(in vec3 min, in vec3 max)
{
	return orthoLH01(min.x, max.x, min.y, max.y, min.z, max.z);
}

mat4 perspectiveFovRH(in float fov, in float width, in float height, in float zNear, in float zFar)
{
	float rad = fov;
	float h = cos(0.5 * rad) / sin(0.5 * rad);
	float w = h * height / width; ///todo max(width , Height) / min(width , Height)?

	mat4  Result = mat4(0);
	Result[0][0] = w;
	Result[1][1] = h;
	Result[2][2] = - (zFar + zNear) / (zFar - zNear);
	Result[2][3] = - 1.;
	Result[3][2] = - (2. * zFar * zNear) / (zFar - zNear);
	return Result;
}

mat4 lookAtLH(vec3 eye, vec3 target, vec3 up)
{
	vec3 f = normalize(target - eye);
	vec3 s = normalize(cross(f, up)); 
	vec3 u = normalize(cross(s, f));

	mat4 view = mat4(1.);
	view[0][0] = s.x;
	view[1][0] = s.y;
	view[2][0] = s.z;
	view[0][1] = u.x;
	view[1][1] = u.y;
	view[2][1] = u.z;
	view[0][2] =-f.x;
	view[1][2] =-f.y;
	view[2][2] =-f.z;
	view[3][0] =-dot(s, eye);
	view[3][1] =-dot(u, eye);
	view[3][2] = dot(f, eye);
	return view;

}
#define PI 3.141592
#define HALF_PI 3.141592 / 2. 

void main() 
{
	vec3 n = normalize(cross(In[1].Position-In[0].Position, In[2].Position-In[0].Position));
	float x = dot(n, vec3(1., 0., 0.));
	float y = dot(n, vec3(0., 1., 0.));
	float z = dot(n, vec3(0., 0., 1.));

	vec3 areaMin = u_pm_info.area_min.xyz;
	vec3 areaMax = u_pm_info.area_max.xyz;
	vec3 size = (areaMax - areaMin);
	vec3 center = size/2. + areaMin;
	vec3 eye = center;
	vec3 up = vec3(0., 1., 0.);
	mat4 projection;

	if(abs(x) > abs(y) && abs(x) > abs(z)){
		eye.x = x > 0 ? areaMax.x : areaMin.x;
		projection = orthoLH01(areaMin.z, areaMax.z, areaMin.y, areaMax.y, 0, size.x);
	}
	else if(abs(y) > abs(z))
	{
		eye.y = y > 0 ? areaMax.y : areaMin.y;
		up = normalize(vec3(0., 0., 1.));
		projection = orthoLH01(areaMin.x, areaMax.x, areaMin.z, areaMax.z, 0, size.y);
	}
	else
	{
		eye.z = z > 0 ? areaMax.z : areaMin.z;
		projection = orthoLH01(areaMin.x, areaMax.x, areaMin.y, areaMax.y, 0, size.z);
	}
	mat4 view = lookAtLH(eye, center, up);

	transform.VertexID[0] = In[0].VertexID;
	transform.VertexID[1] = In[1].VertexID;
	transform.VertexID[2] = In[2].VertexID;
	transform.DrawID = In[0].DrawID;
	transform.InstanceID = In[0].InstanceID;

	// 三角形を大きくしてフラグメントシェーダのテストに入るようにする.なくてもいい？
//	Triangle t = MakeTriangle(In[0].Position, In[1].Position, In[2].Position);
//	t = scaleTriangleV(t, vec3(0.5));

	gl_Position = projection * view * vec4(In[0].Position, 1.);
	transform.Position = In[0].Position;
	EmitVertex();
	gl_Position = projection * view * vec4(In[1].Position, 1.);
	transform.Position = In[1].Position;
	EmitVertex();
	gl_Position = projection * view * vec4(In[2].Position, 1.);
	transform.Position = In[2].Position;
	EmitVertex();
	
	EndPrimitive();
}
