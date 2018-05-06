#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/Math.glsl>

out gl_PerVertex{
	vec4 gl_Position;
};

void main()
{
    vec2 xy = vec2((gl_VertexIndex.xx & ivec2(1, 2)) << ivec2(2, 1));
	gl_Position = vec4(xy * 2. - 1., 0, 1);
}
