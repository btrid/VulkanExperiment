#ifndef BTRLIB_MATH_GLSL
#define BTRLIB_MATH_GLSL

#ifndef quat
#define quat vec4
#endif

#ifndef FLT_EPSIRON
#define FLT_EPSIRON (0.0001)
#endif
#ifndef FLT_EPSIRON_HALF
#define FLT_EPSIRON_HALF (0.0001)
#endif

#ifndef PI
#define PI (3.141592)
#endif

#ifndef HALF_PI
#define HALF_PI (1.570796) 
#endif

float rand(in vec2 co){
    return fract(sin(dot(co.xy, vec2(12.9898,78.233))) * 43758.5453);
}
float rand3(in vec3 co){
    return fract(sin(dot(co.xyz, vec3(12.9898,78.233, 55.77))) * 43758.5453);
}
vec4 rand4(in vec2 co){
	return vec4(rand(co*sin(co.x*co.y)), rand(co.yx), rand(co*sin(co.y)), rand(co*sin(co.x)));
}

mat3 quatToMat3(in vec4 q)
{
	mat3 m = mat3(1.);
	float qxx = q.x * q.x;
	float qyy = q.y * q.y;
	float qzz = q.z * q.z;
	float qxz = q.x * q.z;
	float qxy = q.x * q.y;
	float qyz = q.y * q.z;
	float qwx = q.w * q.x;
	float qwy = q.w * q.y;
	float qwz = q.w * q.z;

	m[0][0] = 1 - 2 * (qyy +  qzz);
	m[0][1] = 2 * (qxy + qwz);
	m[0][2] = 2 * (qxz - qwy);

	m[1][0] = 2 * (qxy - qwz);
	m[1][1] = 1 - 2 * (qxx +  qzz);
	m[1][2] = 2 * (qyz + qwx);

	m[2][0] = 2 * (qxz + qwy);
	m[2][1] = 2 * (qyz - qwx);
	m[2][2] = 1 - 2 * (qxx +  qyy);
	return m;
}

mat4 translate(in mat4 m, in vec3 v)
{
	mat4 ret = m;
	ret[3] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
	return ret;
}

mat4 scaling(in mat4 m, in vec3 v)
{
	mat4 ret = mat4(1.);
	ret[0] = m[0] * v[0];
	ret[1] = m[1] * v[1];
	ret[2] = m[2] * v[2];
	ret[3] = m[3];
	return ret;
}
mat4 scale(in vec3 v)
{
	return scaling(mat4(1.), v);
}

vec4 toQuat(in vec3 eular)
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

vec3 rotate(in quat q, in vec3 v)
{
	vec3 QuatVector = q.xyz;
	vec3 uv = cross(QuatVector, v);
	vec3 uuv = cross(QuatVector, uv);
	uv *= (2. * q.w);
	uuv *= 2.;

	return v + uv + uuv;
}

mat4 ortho(in float l, in float r, in float b, in float t, in float n, in float f)
{
	mat4 o = mat4(1.);
	o[0][0] = 2. / (r - l);
	o[1][1] = 2. / (t - b);
	o[2][2] = -2. / (f - n);
	o[3][0] = - (r + l) / (r - l);
	o[3][1] = - (t + b) / (t - b);
	o[3][2] = - (f + n) / (f - n);
//	o[0][3] = - (r + l) / (r - l);
//	o[1][3] = - (t + b) / (t - b);
//	o[2][3] = - (f + n) / (f - n);
	return o;
}

mat4 lookat(in vec3 eye, in vec3 target, in vec3 up)
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

quat angleAxis(in float angle, in vec3 v)
{
	quat Result;

	float s = sin(angle * 0.5);
	Result.w = cos(angle * 0.5);
	Result.x = v.x * s;
	Result.y = v.y * s;
	Result.z = v.z * s;
	return Result;
}

mat3 eulerAngleXYZ(in vec3 euler)
{
	vec3 c = cos(-euler);
	vec3 s = sin(-euler);
	
	mat3 Result = mat3(1.);
	Result[0][0] = c.y * c.z;
	Result[0][1] =-c.x * s.z + s.x * s.y * c.z;
	Result[0][2] = s.x * s.z + c.x * s.y * c.z;
	Result[1][0] = c.y * s.z;
	Result[1][1] = c.x * c.z + s.x * s.y * s.z;
	Result[1][2] =-s.x * c.z + c.x * s.y * s.z;
	Result[2][0] =-s.y;
	Result[2][1] = s.x * c.y;
	Result[2][2] = c.x * c.y;
	return Result;
}

float calcAngle(in vec3 x, in vec3 y)
{
	return acos(clamp(dot(x, y), -1., 1));
}

vec3 rotateQuatVec3(in quat q,	in vec3 v)
{

	vec3 quat_vector = q.xyz;
	vec3 uv = cross(quat_vector, v);
	vec3 uuv = cross(quat_vector, uv);

	return v + ((uv * q.w) + uuv) * 2.;
}


#endif
