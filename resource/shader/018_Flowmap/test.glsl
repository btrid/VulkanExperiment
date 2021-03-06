float mod289(float x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 mod289(vec4 x){return x - floor(x * (1.0 / 289.0)) * 289.0;}
vec4 perm(vec4 x){return mod289(((x * 34.0) + 1.0) * x);}

float noise(vec3 p)
{
	vec3 a = floor(p);
	vec3 d = p - a;
	d = d * d * (3.0 - 2.0 * d);

	vec4 b = a.xxyy + vec4(0.0, 1.0, 0.0, 1.0);
	vec4 k1 = perm(b.xyxy);
	vec4 k2 = perm(k1.xyxy + b.zzww);

	vec4 c = k2 + a.zzzz;
	vec4 k3 = perm(c);
	vec4 k4 = perm(c + 1.0);

	vec4 o1 = fract(k3 * (1.0 / 41.0));
	vec4 o2 = fract(k4 * (1.0 / 41.0));

	vec4 o3 = o2 * d.z + o1 * (1.0 - d.z);
	vec2 o4 = o3.yw * d.x + o3.xz * (1.0 - d.x);

	return o4.y * d.y + o4.x * (1.0 - d.y);
}


float DropMap(vec3 p)
{
	p*=0.2 * smoothstep(0., 10., iTime);
	float amplification = 1.;
	float v = 0.;
	float c = 0.;
	for(int i = 0; i < 4; i++)
	{
		v = v*amplification + noise(p);
		c = c*amplification + 1.;
		p*=1.3;
	}
	return (v / c)*0.8+0.2;
}

float getData()
{
	vec4 drop = vec4(333.,333.,182.,1.);
	uvec2 uv = uvec2(gl_FragCoord.xy);
	float v = 0.;

	float bounds = smoothstep(10., 0., iTime);

	float dist = distance(vec2(uv)+0.5, drop.xy);
	float influence = drop.z/dist - bounds;
	
	if(influence >= 0.5)
	{
		float dropmap = DropMap(vec3(vec2(uv)+vec2(iTime)*1.5, iTime*1.5));
		influence *= smoothstep(0.5, 0.5, influence*dropmap);

		if(influence > 0.001)
		{
			v += influence * drop.w;
		}
	}		
	return v;

}

float FlowMap(vec3 p)
{
	float amplification = 1.;
	p*= 0.09;
	float v = 0.;
	float c = 0.;
	for(int i = 0; i < 4; i++)
	{
		v = v*amplification + noise(p);
		c = c*amplification + 1.;
		p*=1.3;
	}
	return (v / c)*2.-1.;
}


void main() 
{
	float v = getData();
	uvec2 uv = uvec2(gl_FragCoord.xy);
	{
		float flowmap = FlowMap(vec3(vec2(uv), iTime*5.));
		gl_FragColor = vec4(vec3(v)*flowmap, 1.);
//		gl_FragColor = vec4(vec3(flowmap), 1.);		
	}
	gl_FragColor = vec4(vec3(step(0.01, v)), 1.);


}