
#if defined(SETPOINT_CAMERA)
layout(std140, set=SETPOINT_CAMERA, binding=0) uniform CameraUniform
{
	mat4 uProjection;
	mat4 uView;
	vec4 u_eye;
	vec4 u_target;
	float u_aspect;
	float u_fov_y;
	float u_near;
	float u_far;
};
#endif

float rand(in vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}
