#include <btrlib/Shape.h>

bool Plane::isHit(const AABB& a)const
{
	glm::vec3 c = (a.max_ + a.min_) * 0.5f;
	glm::vec3 e = a.max_ - c;

	float r = glm::dot(e, glm::abs(normal_));
	float d = glm::dot(normal_, c) - dot_;
	return glm::abs(d) <= r;
}

vec2 nearestAABBPoint(const vec4& aabb, const vec2& p)
{
	return min(max(p, aabb.xy()), aabb.zw());
}

bool containsAABBPoint(const vec4& aabb, const vec2& p)
{
	return p.x >= aabb.x && p.x <= aabb.z
		&& p.y >= aabb.y && p.y <= aabb.w;
}

vec2 intersection(vec4 aabb, vec2 p, vec2 dir)
{
	using glm::max; using glm::min;

	vec4 t = (aabb - p.xyxy()) / dir.xyxy();
	vec2 tmin = min(t.xy(), t.zw());

	return p + max(tmin.x, tmin.y) * dir;
}

vec2 intersectionAABBSegment(vec4 aabb, vec2 p, vec2 dir, vec2 inv_dir)
{
	using namespace glm;
	vec4 t = (aabb - p.xyxy) * inv_dir.xyxy;
	vec2 tmin = min(t.xy(), t.zw());
	vec2 tmax = max(t.xy(), t.zw());

	float n = max(tmin.x, tmin.y);
	float f = min(tmax.x, tmax.y);
	//	auto pp = p + (n >= 0. ? n : f) * dir;
	auto pp = p + f * dir;
//	printf("out=[%6.3f,%6.3f], p=[%6.3f,%6.3f], d=[%6.3f,%6.3f], nf=[%6.3f,%6.3f]\n", pp.x, pp.y, p.x, p.y, dir.x, dir.y, n, f);
	return pp;
}

vec2 closest(const vec4 seg, const vec2& p)
{
	auto ab = seg.zw() - seg.xy();
	float t = dot(p - seg.xy(), ab) / dot(ab, ab);
	t = glm::clamp(t, 0.f, 1.f);
	return seg.xy() + t * ab;
}

vec2 inetsectLineCircle(const vec2& p, const vec2& d, const vec2& cp)
{
	//https://stackoverflow.com/questions/1073336/circle-line-segment-collision-detection-algorithm
	vec2 result = p;
	float t1 = 0.f, t2 = 0.f;
	float r = 0.5f;
	float a = dot(d, d);
	float b = 2.f * dot(p - cp, d);
	float c = dot(p - cp, p - cp) - r * r;

	float discriminant = b * b - 4 * a * c;
	float l = distance(p, cp);
	if (distance(p, cp) > r)
	{
		int _ = 0;
	}
	else if (discriminant < 0)
	{
		// no intersection
		int _ = 0;
	}
	else
	{
		// ray didn't totally miss sphere,
		// so there is a solution to
		// the equation.
		discriminant = sqrt(discriminant);

		// either solution may be on or off the ray so need to test both
		// t1 is always the smaller value, because BOTH discriminant and
		// a are nonnegative.
		t1 = (-b - discriminant) / (2.f * a);
		t2 = (-b + discriminant) / (2.f * a);

		// 3x HIT cases:
		//          -o->             --|-->  |            |  --|->
		// Impale(t1 hit,t2 hit), Poke(t1 hit,t2>1), ExitWound(t1<0, t2 hit), 

		// 3x MISS cases:
		//       ->  o                     o ->              | -> |
		// FallShort (t1>1,t2>1), Past (t1<0,t2<0), CompletelyInside(t1<0, t2>1)

		if (t1 >= 0 && t1 <= 1)
		{
			// t1 is the intersection, and it's closer than t2
			// (since t1 uses -b - discriminant)
			// Impale, Poke
//			return true;
		}

		// here t1 didn't intersect so we are either started
		// inside the sphere or completely past it
		if (t2 >= 0 && t2 <= 1)
		{
			// ExitWound
//			return true;
		}

		// no intn: FallShort, Past, CompletelyInside
//		return false;
		if (t1 >= 0.f)
			result = p + d * t1;
		else
			result = p + d * t2;
	}

//	printf("out=[%6.3f,%6.3f], p=[%6.3f,%6.3f], cp=[%6.3f,%6.3f], d=[%6.3f,%6.3f], nf=[%6.3f,%6.3f] dist=[%6.3f], %s\n", result.x, result.y, p.x, p.y, cp.x, cp.y, d.x, d.y, t1, t2, l, glm::all(glm::epsilonEqual(p, result, 0.001f)) ? "not hit" : "hit");
	return result;
}


#include <glm/gtx/rotate_vector.hpp>
#include <array>
#include <vector>
void RayTracing::dda(const ivec2& begin, const ivec2& end)
{
	ivec2 delta = abs(end - begin);
	ivec3 _dir = glm::sign(ivec3(end, 0) - ivec3(begin, 0));

	int axis = delta.x >= delta.y ? 0 : 1;
	ivec2 dir[2];
	dir[0] = _dir.xz();
	dir[1] = _dir.zy();

	{
		int	D = 2 * delta[1 - axis] - delta[axis];
		ivec2 pos = begin;
		for (; true;)
		{
			if (D > 0)
			{
				pos += dir[1 - axis];
				D = D - 2 * delta[axis];
			}
			else
			{
				pos += dir[axis];
				D = D + 2 * delta[1 - axis];
			}

			printf("pos=[%3d,%3d]\n", pos.x, pos.y);
			if (all(glm::equal(pos, begin)))
			{
				break;
			}
		}
		printf("pos\n");
	}
	float dist = distance(vec2(end), vec2(begin));
	float p = 1. / (1. + dist * dist * 0.01);
	auto target = vec2(begin) + vec2(end - begin) * p * 100.f;
	dist += 1.;
}

void RayTracing::dda_test()
{
	std::array<int, 32 * 32> data{};
	ivec3 reso = ivec3(32);
	for (int z = 0; z < 32; z++)
	{
		auto ray = glm::rotateZ(glm::vec3(0.f, 1.f, 0.f), glm::radians(180.f) / 32.f * (z + 0.5f));
		ivec2 delta = abs(ray * vec3(reso)).xy();
		ivec3 _dir = ivec3(glm::sign(ray));

		int axis = delta.x >= delta.y ? 0 : 1;
		ivec2 dir[2];
		dir[0] = _dir.xz();
		dir[1] = _dir.zy();

		for (int y = 0; y < reso.y; y++)
		{
			for (int x = 0; x < reso.x; x++)
			{
				int	D = 2 * delta[1 - axis] - delta[axis];
				ivec2 pos;
				pos.x = _dir.x >= 0 ? 0 : reso.x - 1;
				pos.y = _dir.y >= 0 ? 0 : reso.y - 1;
				for (; true;)
				{
					if (D > 0)
					{
						pos += dir[1 - axis];
						D = D - 2 * delta[axis];
					}
					else
					{
						pos += dir[axis];
						D = D + 2 * delta[1 - axis];
					}

					data[y * reso.x + x]++;
					if (any(glm::lessThan(pos, ivec2(0))) || any(glm::greaterThanEqual(pos, reso.xy())))
					{
						break;
					}
				}
			}
		}
	}
	printf("");
}

void RayTracing::makeCircle(int Ox, int Oy, int R)
{
	int S = 128;
	std::vector<char> field(S * S);
	int x = R;
	int y = 0;
	int err = 0;
	int dedx = (R << 1) - 1;	// 2x-1 = 2R-1
	int dedy = 1;	// 2y-1

	field[(Ox + R) + Oy * S] = 1;// +0
	field[Ox + (Oy + R) * S] = 1;// +90
	field[(Ox - R) + Oy * S] = 1;// +180
	field[Ox + (Oy - R) * S] = 1;// +270

	while (x > y)
	{
		y++;
		err += dedy;
		dedy += 2;
		if (err >= dedx)
		{
			x--;
			err -= dedx;
			dedx -= 2;
		}

		field[(Ox + x) + (Oy + y) * S] = 1;// +theta
		field[(Ox + x) + (Oy - y) * S] = 1;// -theta
		field[(Ox - x) + (Oy + y) * S] = 1;// 180-theta
		field[(Ox - x) + (Oy - y) * S] = 1;// 180+theta
		field[(Ox + y) + (Oy + x) * S] = 1;// 90+theta
		field[(Ox + y) + (Oy - x) * S] = 1;// 90-theta
		field[(Ox - y) + (Oy + x) * S] = 1;// 270+theta
		field[(Ox - y) + (Oy - x) * S] = 1;// 270-theta
	}

	for (int _y = 0; _y < S; _y++)
	{
		for (int _x = 0; _x < S; _x++)
		{
			printf("%c", field[_x + _y * S] ? '@' : ' ');
		}
		printf("\n");
	}
}
