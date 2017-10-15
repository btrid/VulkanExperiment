#ifndef BTRLIB_COMMON_GLSL
#define BTRLIB_COMMON_GLSL

#define quat vec4
#define FLT_EPSIRON (0.0001)

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

struct DrawIndirectCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

struct DrawElementIndirectCommand{
	uint  count;
	uint  instanceCount;
	uint  firstIndex;
	uint  baseVertex;
	uint  baseInstance;
};


float rand(in vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
vec4 rand4(in vec2 co){
	return vec4(rand(co*sin(co.x*co.y)), rand(co.yx), rand(co*sin(co.y)), rand(co*sin(co.x)));
}

#endif