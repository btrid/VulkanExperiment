#version 450

#extension GL_ARB_shader_draw_parameters : require
#extension GL_GOOGLE_cpp_style_line_directive : require

#include <btrlib/Math.glsl>

out gl_PerVertex{
	vec4 gl_Position;
};

layout(location=1) out ModelData
{
	vec2 texcoord;
}out_modeldata;

void main()
{
    float x = -1.0 + float((gl_VertexIndex & 1) << 2);
    float y = -1.0 + float((gl_VertexIndex & 2) << 1);
    out_modeldata.texcoord.x = (x+1.0)*0.5;
    out_modeldata.texcoord.y = (y+1.0)*0.5;
	gl_Position = vec4(x, y, 0, 1);
}
