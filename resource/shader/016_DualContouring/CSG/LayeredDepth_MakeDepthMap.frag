#version 460
#extension GL_GOOGLE_include_directive : require

#define USE_LayeredDepth 0
#include "CSG.glsl"

#define SETPOINT_CAMERA 1
#include "btrlib/Camera.glsl"

layout(location = 0) out vec4 FragColor;
///*layout (depth_greater)*/ out float gl_FragDepth;
void main()
{
//	gl_FragDepth = 0.;
	FragColor = vec4(vec3(0.5), 1.);

	uvec3 texsize = uvec3(1024, 1024, 1);
	uvec2 index = uvec2(gl_FragCoord.xy);
	uint i = index.x + index.y*texsize.x;
	int head = b_LayeredDepth_point_link_head[i];
	if(head < 0)
	{
		discard;
	}
	gl_FragDepth = 0.;
	if(head >= 0)
	{
		Camera cam = u_camera[0];
		vec3 f = normalize(cam.u_target.xyz - cam.u_eye.xyz).xyz;
		vec3 s = normalize(cross(cam.u_up.xyz, f));
		vec3 u = cam.u_up.xyz;

		// イメージセンサー上の位置
		float tan_fov_y = tan(cam.u_fov_y / 2.);
		vec2 screen = (index / vec2(texsize.xy)).xy * 2. - 1.;

		vec3 dir = normalize(f + s * screen.x * tan_fov_y * cam.u_aspect + u * screen.y * tan_fov_y);
		vec3 origin = cam.u_eye.xyz;

		vec4 p = (cam.u_projection *vec4(dir*b_LayeredDepth_point[head].p, 1.));
		gl_FragDepth = p.z/p.w;
	}

}