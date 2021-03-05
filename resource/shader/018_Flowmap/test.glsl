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

float remap(float value, float inputMin, float inputMax, float outputMin, float outputMax)
{
    return (value - inputMin) * ((outputMax - outputMin) / (inputMax - inputMin)) + outputMin;
}

float DropMap(vec3 p)
{
	float amplification = 1.;
	p*= 0.03;
	float v = 0.;
	float c = 0.;
	for(int i = 0; i < 4; i++)
	{
		v = v*amplification + noise(p);
		c = c*amplification + 1.;
		p*=1.3;
	}
	return (v / c);
	return 1.-exp(-(v / c));

}

float FlowMap(vec3 p)
{
	float amplification = 1.2;
	p*= 0.03;
	float v = 0.;
	float c = 0.;
	for(int i = 0; i < 4; i++)
	{
		v = v*amplification + noise(p);
		c = c*amplification + 1.;
		p*=1.53;
	}
	return (v / c)*0.2+0.8;
}


void main() 
{
	vec4 points[3];
	points[0] = vec4(333.,333.,182.,1.);
	points[1] = vec4(444.,444.,182.,-1.);
	points[2] = vec4(551.,355.,182.,-1.);
	uvec2 uv = uvec2(gl_FragCoord.xy);
	float v = 0.;

	for(int i = 0; i < points.length(); i++)
	{
		vec4 drop = points[i];
		float dist = distance(vec2(uv)+0.5, drop.xy);
		float influence = drop.z/dist;
		
		if(influence >= 0.5)
		{
			float dropmap = DropMap(vec3(uv, i));

			influence *= smoothstep(0.5, 0.5, influence*dropmap);

			// 違うところへ塗るには影響力を小さくする?
//			float is_same = float(v!=0. || sign(v) == sign(drop.w));
//			influence *= ((1.-is_same)*0.5+0.5);
			if(influence > 0.1)
			{
				v = influence * drop.w;
			}

		}
		
	}

	{
		vec3 color1 = vec3(1.0, 0.5, 0.5);
		vec3 color2 = vec3(0.5, 0.5, 1.0);
		vec3 color = vec3(0.);

		color = mix(color, color1, smoothstep(0., 0., v));
		color = mix(color, color2, smoothstep(0., 0., -v));

		float flowmap = FlowMap(vec3(vec2(uv)+vec2(iTime)*10., iTime*5.));


		gl_FragColor = vec4(color, 1.);

//		gl_FragColor = vec4(vec3(flowmap), 1.);
		
	}


}