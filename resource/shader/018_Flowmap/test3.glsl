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


vec2 rotate(float angle)
{
	vec2 v = vec2(-1., 0.);
	float c = cos(angle);
	float s = sin(angle);
	return vec2(v.x*c-v.y*s, v.x*s+v.y*c);
}
vec2 DiffusionMap(vec3 p)
{
	float amplification = 1.;
	p*= 0.03;
	vec2 v = vec2(0.);
	vec2 c = vec2(0.);
	for(int i = 0; i < 4; i++)
	{
		v = v*amplification + vec2(noise(p), noise(p+1000.));
		c = c*amplification + 1.;
		p*=1.3;
	}
	return v / c;

}

float DropMap(vec3 p)
{
	float amplification = 1.;
	p*= 0.01;
	float v = 0.;
	float c = 0.;
	for(int i = 0; i < 4; i++)
	{
		v = v*amplification + noise(p);
		c = c*amplification + 1.;
		p*=1.51;
	}
	return v / c;
}

float getData(vec2 uv)
{
	vec4 drop = vec4(333.,333.,182.,1.);

	float v = 0.;

	float dist = distance(uv, drop.xy);
	float influence = drop.z/dist;
	
	if(influence >= 0.5)
	{
		float dropmap = DropMap(vec3(uv*0.1, 0.));
		influence *= smoothstep(0.5, 0.5, influence*dropmap);

		if(influence > 0.001)
		{
			v += influence * drop.w;
		}
	}		
	return v;

}
void main() 
{
	vec2 uv = vec2(gl_FragCoord.xy+0.5);
	float flow = DropMap(vec3(uv*2., iTime*10.5));
	float flow_x = DropMap(vec3(uv*2.*flow, (flow*12.5+13.5)*flow*7.1));
	float flow_y = DropMap(vec3(uv*2.*flow+vec2(100.), (flow*12.5+13.5)*flow*5.5));

	uv += vec2(flow_x, flow_y) * 200. - 100.;
//	uv DiffusionMap(vec3(uv*2.+flow*2., flow*30.5))*80.-40.;

	float v = getData(uv);
	gl_FragColor = vec4(vec3(v), 1.);
}