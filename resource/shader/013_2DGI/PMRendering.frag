#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/ConvertDimension.glsl"
#include "btrlib/Common.glsl"
#include "btrlib/Math.glsl"

#define USE_GI2D 0
#define USE_GI2D_RENDER 1
#include "GI2D.glsl"



layout(location = 0) out vec4 FragColor;
void main()
{
	vec3 rgb = vec3(0.);
	uvec2 reso = textureSize(s_color[0], 0).xy;
	vec2 uv = gl_FragCoord.xy/reso;
	rgb.rgb += texture(s_color[0], uv).rgb;
	FragColor = vec4(rgb, 1.);

}