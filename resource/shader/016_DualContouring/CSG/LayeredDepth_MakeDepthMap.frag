#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LayeredDepth 0
#include "CSG.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

// depthSample from depthTexture.r, for instance
float linearDepth(float depthSample)
{
//	depthSample = 2.0 * depthSample - 1.0;
//	float zLinear = 2.0 * zNear * zFar / (zFar + zNear - depthSample * (zFar - zNear));
//	return zLinear;
	return 0;
}

// result suitable for assigning to gl_FragDepth
float depthSample(float linearDepth)
{
	Camera cam = u_camera[0];
	float nonLinearDepth = (cam.u_far + cam.u_near - 2.0 * cam.u_near * cam.u_far / linearDepth) / (cam.u_far - cam.u_near);
	nonLinearDepth = (nonLinearDepth + 1.0) / 2.0;
	return nonLinearDepth;
}

void main()
{
	uvec3 texsize = uvec3(1024, 1024, 1);
	uvec2 index = uvec2(gl_FragCoord.xy);
	uint i = index.x + index.y*texsize.x;
	int head = b_LayeredDepth_point_link_head[i];
	gl_FragDepth = head >= 0 ? depthSample(b_LayeredDepth_point[head].p) : 1.;

}