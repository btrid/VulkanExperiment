
#define quat vec4
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

float calcAngle(in vec3 x, in vec3 y)
{
	return acos(clamp(dot(x, y), -1., 1));
}

vec3 rotateQuatVec3(in quat q,	in vec3 v)
{

	vec3 quat_vector = vec3(q.x, q.y, q.z);
	vec3 uv = cross(quat_vector, v);

	vec3 uuv = cross(quat_vector, uv);

	return v + ((uv * q.w) + uuv) * 2.;
}

#define FLT_EPSIRON (0.0001)
struct DrawIndirectCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

#if defined(SETPOINT_CAMERA)
layout(std140, set=SETPOINT_CAMERA, binding=0) uniform CameraUniform
{
	mat4 uProjection;
	mat4 uView;
};
#endif

float rand(in vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

#define INVALID_INDEX 0xFFFFFFFF