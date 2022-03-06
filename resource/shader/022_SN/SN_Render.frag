#version 460
#extension GL_GOOGLE_include_directive : require

layout(location=1) in PerVertex
{
	vec3 WorldPos;
	vec3 Normal;
	vec2 Texcoord_0;
}In;

layout(location = 0) out vec4 FragColor;

void main() 
{
	FragColor = vec4(1.0);
/*    vec3 L = normalize(v_vert - u_lightPos);
    float NL = dot(normalize(v_vertNormal), L);
    if (gl_FrontFacing) 
	{
        if (v_frontColor.a == 0.0) discard;
 		gl_FragColor = clamp(v_frontColor * 0.4 + v_frontColor * 0.6 * NL, 0.0, 1.0);
    }
 	else 
	 {
        if (v_backColor.a == 0.0) discard;
        gl_FragColor = clamp(v_backColor * 0.4 - v_backColor * 0.6 * NL, 0.0, 1.0);
    }
*/
}