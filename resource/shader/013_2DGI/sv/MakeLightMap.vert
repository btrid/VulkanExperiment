#version 450
#extension GL_GOOGLE_include_directive : require

#include "btrlib/Math.glsl"

#define USE_SV 0
#define USE_SV_RENDER 1
#include "SV.glsl"

out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out VS
{
	vec2 texcoord;
}vs_out;

void main()
{
    vec2 xy = vec2((gl_VertexIndex.xx & ivec2(1, 2)) << ivec2(2, 1));
	gl_Position = vec4(xy * 2. - 1., 0., 1.);
	vs_out.texcoord = xy;

}
