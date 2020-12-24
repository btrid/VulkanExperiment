#pragma once

vec2 sign_not_zero(vec2 v)
{
	return glm::step(vec2(0.f), v) * vec2(2.0) + vec2(-1.0);
}
uint pack_normal_octahedron(vec3 _v)
{
	vec3 v = vec3(_v.xy() / dot(abs(_v), vec3(1)), _v.z);
	return packHalf2x16(mix(v.xy(), (1.f - abs(v.yx())) * sign_not_zero(v.xy()), glm::step(v.z, 0.f)));

}
vec3 unpack_normal_octahedron(uint packed_nrm)
{
	vec2 nrm = glm::unpackHalf2x16(packed_nrm);
	vec3 v = vec3(nrm.xy(), 1.f - abs(nrm.x) - abs(nrm.y));
	v.xy = glm::mix(v.xy(), (1.f - abs(v.yx())) * sign_not_zero(v.xy()), glm::step(v.z, 0.f));
	return normalize(v);
}

float determinant(
	float a, float b, float c,
	float d, float e, float f,
	float g, float h, float i)
{
	return glm::determinant(mat3(a, b, c, d, e, f, g, h, i));
}


// Solves for x in  A*x = b.
// 'A' contains the matrix row-wise.
// 'b' and 'x' are column vectors.
// Uses cramers rule.

vec3 solve3x3(const float* A, const float b[3])
{
	auto det = determinant(
		A[0 * 3 + 0], A[0 * 3 + 1], A[0 * 3 + 2],
		A[1 * 3 + 0], A[1 * 3 + 1], A[1 * 3 + 2],
		A[2 * 3 + 0], A[2 * 3 + 1], A[2 * 3 + 2]);

	if (abs(det) <= 1e-12)
	{
		return vec3(NAN);
	}

	return vec3
	{
		determinant(
			b[0],    A[0 * 3 + 1], A[0 * 3 + 2],
			b[1],    A[1 * 3 + 1], A[1 * 3 + 2],
			b[2],    A[2 * 3 + 1], A[2 * 3 + 2]),

		determinant(
			A[0 * 3 + 0], b[0],    A[0 * 3 + 2],
			A[1 * 3 + 0], b[1],    A[1 * 3 + 2],
			A[2 * 3 + 0], b[2],    A[2 * 3 + 2]),

		determinant(
			A[0 * 3 + 0], A[0 * 3 + 1], b[0]   ,
			A[1 * 3 + 0], A[1 * 3 + 1], b[1]   ,
			A[2 * 3 + 0], A[2 * 3 + 1], b[2])

	} / det;
}


// Solves A*x = b for over-determined systems.
// Solves using  At*A*x = At*b   trick where At is the transponate of A
vec3 leastSquares(size_t N, const vec3* A, const float* b)
{
	float At_A[3][3];
	float At_b[3];

	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			float sum = 0;
			for (size_t k = 0; k < N; ++k)
			{
				sum += A[k][i] * A[k][j];
			}
			At_A[i][j] = sum;
		}
	}

	for (int i = 0; i < 3; ++i)
	{
		float sum = 0;

		for (size_t k = 0; k < N; ++k)
		{
			sum += A[k][i] * b[k];
		}

		At_b[i] = sum;
	}

	return solve3x3(&At_A[0][0], At_b);
}

vec3 IntersectPlanes(vec4 plane[3])
{
	vec3 u = cross(plane[1].xyz(), plane[2].xyz());
	float denom = dot(plane[0].xyz(), u);
	if (abs(denom) < 0.00001f) { return vec3(NAN); }
	return (plane[0].w * u + cross(plane[0].xyz(), plane[2].w * plane[1].xyz() - plane[1].w * plane[2].xyz())) / denom;

}
void test()
{
	{
		// 		for ( int i = 0; i < 100; i++)
		// 		{
		// 			auto n1 = normalize(glm::ballRand(10.f));
		// 			auto n2 = unpack_normal_octahedron(pack_normal_octahedron(n1));
		// 			printf("%3d %8.3f, %8.3f, %8.3f\n    %8.3f, %8.3f, %8.3f\n", i, n1.x, n1.y, n1.z, n2.x, n2.y, n2.z);
		// 
		// 		}

	}

	ivec3 cell = ivec3(0, 0, 0);
	for (int a = 0; a < 1000; a++)
	{
		vec3 normal[30] = {};
		float d[30];
		std::tuple<bool, vec3, float> Hit[30];
		int count = 0;
		Plane P(glm::linearRand(vec3(0.f), vec3(1.f)) + vec3(cell), glm::linearRand(vec3(0.f), vec3(1.f)) + vec3(cell), glm::linearRand(vec3(0.f), vec3(1.f)) + vec3(cell));

		int ishit[3] = {};
		vec3 p = {};


		for (int z = 0; z < 2; z++)
		for (int y = 0; y < 2; y++)
		for (int x = 0; x < 2; x++)
		for (int i = 0; i < 3; i++)
		{
			ivec3 n = ivec3(i == 0, i == 1, i == 2);
			ivec3 p = ivec3(x, y, z);
			if (any(equal(n & p, ivec3(1)))) { continue; }
			p += cell;

			auto hit = P.intersect(p, p + n);
			if (!std::get<0>(hit)) { continue; }
			if (std::get<2>(hit) < 0.f || std::get<2>(hit) >= 1.f) { continue; }
			normal[count] = P.normal_;
			d[count] = dot(P.normal_, std::get<1>(hit));
			Hit[count] = hit;
			count++;
		}

		if (count == 0) { printf("no hit\n");  continue; }

		for (int i = 0; i < 3; i++)
		{
			vec3 pp = vec3(1.f);
			vec3 n = vec3(i == 0, i == 1, i == 2) * pp;

			normal[count] = n;
			d[count] = dot(n, vec3(cell) + vec3(0.5f));
			count++;
		}

		auto _ = leastSquares(count, normal, d);
		auto ppp = P.getNearest(_);
		auto nearp = P.getNearest(vec3(cell) + vec3(0.5f));
		printf("lstSq=[%6.3f,%6.3f,%6.3f] OnPlane=[%6.3f,%6.3f,%6.3f] NearPlane=[%6.3f,%6.3f,%6.3f]\n", _.x, _.y, _.z, ppp.x, ppp.y, ppp.z, nearp.x, nearp.y, nearp.z);
		for (int i = 0; i < count - 3; i++)
		{
			auto& hitp = std::get<1>(Hit[i]);
			printf(" hitp%2d=[%6.3f,%6.3f,%6.3f]\n", i, hitp.x, hitp.y, hitp.z);
		}

		if (isnan(_.x))
		{
			int ___ = 0;
		}
	}
}
