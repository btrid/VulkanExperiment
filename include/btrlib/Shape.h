#pragma once
#include <btrlib/DefineMath.h>
#include <algorithm>
#include <tuple>
#include <vector>
struct Rect{
	glm::vec2 lt_;
	glm::vec2 rb_;
};

struct Point2
{
	glm::vec2 p_;
};

struct Segment2 
{
	glm::vec2 a_;
	glm::vec2 b_;

	glm::vec2 dir()const{ return glm::normalize(b_ - a_); }
	bool isHit(const Segment2& rhv)const{	
		auto s = glm::cross(glm::vec3(dir(), 0.f), glm::vec3(a_ - rhv.a_, 0.f));
		auto e = glm::cross(glm::vec3(dir(), 0.f), glm::vec3(a_ - rhv.b_, 0.f));
		return s.z * e.z < 0.f;
	}

	glm::vec2 closest(const glm::vec2& c)const
	{
		auto ab = b_ - a_;
		float t = glm::dot(c - a_, ab) / glm::dot(ab, ab);
		glm::clamp(t, 0.f, 1.f);
		return a_ + t*ab;
	}

	float distance(const glm::vec2& c)const
	{
		glm::distance(closest(c), c);
	}

	glm::vec2 closest(const Segment2& rhv, glm::vec2* rhvPoint)const
	{
		float s, t;
		auto d1 = b_ - a_;
		auto d2 = rhv.b_ - rhv.a_;
		auto r = a_ - rhv.a_;
		float a = glm::dot(d1, d1);
		float e = glm::dot(d2, d2);
		float f = glm::dot(d2, r);

		if (a <= FLT_EPSILON && e <= FLT_EPSILON){
			// 両方の線分が点に縮退
			if (rhvPoint){
				*rhvPoint = rhv.a_;
			}
			return a_;
		}
		if (a <= FLT_EPSILON){
			// 線分が点に縮退
			s = 0.f;
			t = f / e;
			t = glm::clamp(t, 0.f, 1.f);
		}
		else{
			float c = glm::dot(d1, r);
			if (e <= FLT_EPSILON)
			{
				// rhvが点になってる
				t = 0.f;
				s = glm::clamp(-c / a, 0.f, 1.f);
			}
			else{
				float b = glm::dot(d1, d2);
				float denom = a*e - b*b;
				if (denom != 0.f){
					s = glm::clamp((b*f - c*e) / denom, 0.f, 1.f);
				}
				else{
					s = 0.f;
				}
				t = (b*s + f) / e;
				if (t <= 0.f){
					t = 0.f;
					s = glm::clamp(-c / a, 0.f, 1.f);
				}
				else if (t > 1.f)
				{
					t = 1.f;
					s = glm::clamp((b - c) / a, 0.f, 1.f);
				}
			}
		}
		if (rhvPoint){
			*rhvPoint = rhv.a_ + d2 * t;
		}
		return a_ + d1 * s;

	}
	float distance(const Segment2& rhv)const
	{
		glm::vec2 p;
		return glm::distance(closest(rhv, &p), p);
	}

	glm::vec2 normal()const{
		return glm::normalize(glm::rotate(dir(), glm::radians(90.f)));
	}
};

struct Triangle2
{
	glm::vec2 a_, b_, c_;
	glm::vec2 ab()const{ return a_ - b_; }
	glm::vec2 bc()const{ return b_ - c_; }
	glm::vec2 ca()const{ return c_ - a_; }
	bool isFrontFace()const
	{
		auto ab = b_ - a_;
		auto ac = c_ - a_;
		return glm::cross(glm::vec3(ab, 0.f), glm::vec3(ac, 0.f)).z > 0.f;
	}
/*	glm::vec2 isIn(const glm::vec2& p)const
	{
		p.x
	}
*/
};
struct Box{

};

struct AABB;
struct Hit
{
	bool	mIsHit;
	glm::vec3 mHitPoint;
	glm::vec3 mWeight;
	float	mDistance;

	Hit()
		: mIsHit(false)
		, mDistance(0.f)
	{}

	static Hit NotHit(){
		return Hit();
	}
};

struct Ray
{
	glm::vec3 p_;
	glm::vec3 d_;

	Ray(){}
	Ray(const glm::vec3& p, const glm::vec3 d)
		: p_(p)
		, d_(d)
	{}

//	Segment
};

struct Triangle
{
	enum ClosestPoint{
		CLOSEST_A,
		CLOSEST_B,
		CLOSEST_C,
		CLOSEST_AB,
		CLOSEST_BC,
		CLOSEST_AC,
		CLOSEST_ABC,	//!< 内側
	};
	glm::vec3 a_, b_, c_;


	Triangle() = default;
	Triangle(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
		: a_(a), b_(b), c_(c)
	{}
	//! 縮退三角形？
	bool isDegenerate()const{
		// 面積が0なら縮退かな
		auto ab = b_ - a_;
		auto ac = c_ - a_;
		return glm::length(glm::cross(ab, ac)) < FLT_EPSILON;
	}

	bool isFrontFace(const glm::vec3& p)const
	{
		glm::vec3 n = getNormal();
//		glm::vec3 c = getCenter();
		glm::vec3 c = a_;
		glm::vec3 ray = c - p;
		// 法線と座標pからcenterに向けての線分が互い違いならfront
		bool ret = glm::dot(ray, n) < 0.f;
		return ret;
	}

	void scale(float factor) {
		// すげえ適当
		auto l1 = normalize((a_ - b_) + (a_ - c_));
		auto l2 = normalize((b_ - a_) + (b_ - c_));
		auto l3 = normalize((c_ - b_) + (c_ - a_));
		a_ += l1*factor;
		b_ += l2*factor;
		c_ += l3*factor;

	}
	Triangle getBack()const{
		return Triangle{a_, c_, b_};
	}

	Hit intersect(const Ray& ray)const
	{
		// p191
		// 縮退3角形はそもそも面積がないので自分でチェックして
		assert(!isDegenerate());

		auto ab = b_ - a_;
		auto ac = c_ - a_;
		auto qp = -ray.d_;// p - q

		auto n = glm::cross(ab, ac);

		float d = glm::dot(qp, n);
		if (d <= 0.f){
			// 平行か逆向き
			return Hit::NotHit();
		}

		auto ap = ray.p_ - a_;
		float t = glm::dot(ap, n);
		if (t < 0.f){ 
			// 向きが後ろ向き
			return Hit::NotHit();
		}
		// 光線なら t >= 0.f, 線分は 0.f <= t <= 1.fで交差
//		if (t > d){
//			return std::make_tuple<bool, glm::vec3, float>(false, glm::vec3(0.f), 0.f); 
//		}

		glm::vec3 e = glm::cross(qp, ap);
		float v = glm::dot(ac, e);
		if (v <= 0.f || v >= d){
			return Hit::NotHit();
		}
		float w = -glm::dot(ab, e);
		if (w <= 0.f || v + w >= d){
			return Hit::NotHit();
		}

		float ood = 1.f / d;
		t *= ood;
		v *= ood;
		w *= ood;
		float u = 1.f- v - w;
		Hit hit;
		hit.mIsHit = true;
		hit.mWeight = glm::vec3(u, v, w);
		hit.mDistance = t;
		hit.mHitPoint = a_*u + b_ * v + c_ * w;
		return hit;
	}

	glm::vec3 getClosest(const glm::vec3& p)const
	{
		auto ab = b_ - a_;
		auto ac = c_ - a_;
		auto ap = p - a_;

		// 点Aの外側の頂点領域の中にあるかチェック
		float d1 = glm::dot(ab, ap);
		float d2 = glm::dot(ac, ap);
		if (d1 <= 0.f && d2 <= 0.f) {
			return a_;
		}

		// 点Bの外側の頂点領域の中にあるかチェック
		auto bp = p - b_;
		float d3 = glm::dot(ab, bp);
		float d4 = glm::dot(ac, bp);
		if (d3 >= 0.f && d4 <= d3) {
			return b_;
		}

		float vc = d1*d4 - d3*d2;
		if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
			float v = d1 / (d1 - d3);
			return a_ + v* ab;
		}

		auto cp = p - c_;
		float d5 = glm::dot(ab, cp);
		float d6 = glm::dot(ac, cp);
		if (d6 >= 0.f && d5 <= d6)
		{
			return c_;
		}

		float vb = d5*d2 - d1*d6;
		if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
		{
			float w = d2 / (d2 - d6);
			return a_ + w * ac;
		}

		float va = d3*d6 - d5 *d4;
		if (va <= 0.f && d4 - d3 >= 0.f && d5 - d6 >= 0.f)
		{
			float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
			return b_ + w*(c_ - b_);
		}

		// 面の中にある
		float denom = 1.f / (va + vb + vc);
		float v = vb*denom;
		float w = vc*denom;
		return a_ + ab * v + ac * w;

	}

	ClosestPoint getClosestPoint(const glm::vec3& p)const
	{

		auto ab = b_ - a_;
		auto ac = c_ - a_;
		auto ap = p - a_;

		// 点Aの外側の頂点領域の中にあるかチェック
		float d1 = glm::dot(ab, ap);
		float d2 = glm::dot(ac, ap);
		if (d1 <= 0.f && d2 <= 0.f) {
			return CLOSEST_A;
		}

		// 点Bの外側の頂点領域の中にあるかチェック
		auto bp = p - b_;
		float d3 = glm::dot(ab, bp);
		float d4 = glm::dot(ac, bp);
		if (d3 >= 0.f && d4 <= d3) {
			return CLOSEST_B;
		}

		float vc = d1*d4 - d3*d2;
		if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
			return CLOSEST_AB;
		}

		auto cp = p - c_;
		float d5 = glm::dot(ab, cp);
		float d6 = glm::dot(ac, cp);
		if (d6 >= 0.f && d5 <= d6)
		{
			return CLOSEST_C;
		}

		float vb = d5*d2 - d1*d6;
		if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
		{
			return CLOSEST_AC;
		}

		float va = d3*d6 - d5 *d4;
		if (va <= 0.f && d4 - d3 >= 0.f && d5 - d6 >= 0.f)
		{
			return CLOSEST_BC;
		}

		return CLOSEST_ABC;
	}
	float getDistance(const glm::vec3& p)const
	{
		return glm::distance(getClosest(p), p);
	}

	glm::vec3 getNormal()const
	{
		return glm::normalize(glm::cross(glm::normalize(b_ - a_), glm::normalize(c_ - a_)));
	}

	glm::vec3 getCenter()const
	{
		return (a_ + b_ + c_) / 3.f;
	}

	float getArea()const
	{
		return glm::length(glm::cross(b_ - a_, c_ - a_)) * 0.5f;
	}
};

struct Plane
{
	glm::vec3 normal_;
	float dot_;
	Plane()
	{}
	
	Plane(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
	{
		normal_ = glm::normalize(glm::cross(b - a, c - a));
		dot_ = glm::dot(normal_, a);
	}

	Plane(const glm::vec3& point, const glm::vec3& norm)
		: normal_(norm)
	{
		dot_ = glm::dot(normal_, point);
	}

	std::tuple<bool, glm::vec3, float> intersect(const glm::vec3& a, const glm::vec3& b)
	{
		glm::vec3 dir = b - a;
		float dot = glm::dot(normal_, dir);
		if (dot <= 0.f){
			return std::make_tuple(false, glm::vec3(0.f), -1.f);
		}
		float t = (dot_ - glm::dot(normal_, a)) / dot;
		return std::make_tuple(true, a + t*dir, t);
	}

	glm::vec3 getNearest(const glm::vec3& p) const
	{
		float t = getDistance(p);
		return p - t*normal_;
	}

	float getDistance(const glm::vec3& p) const
	{
		assert(glm::isNormalized(normal_, FLT_EPSILON));
		return glm::dot(p, normal_) - dot_;
	}

	bool isHit(const AABB& a)const;
};

struct AABB
{
	glm::vec3 min_;
	glm::vec3 max_;

	AABB()
		: min_(10e10f)
		, max_(-10e10f)
	{}

	AABB(const glm::vec3& min, const glm::vec3& max)
		: min_(min)
		, max_(max)
	{}

	bool test(const AABB& other)
	{
		//		return glm::all(glm::lessThan(max_, other.getMin()))
		//			|| glm::all(glm::lessThan(min_, other.getMax()));

		if (max_.x < other.getMin().x || min_.x > other.getMax().x) { return false; }
		if (max_.y < other.getMin().y || min_.y > other.getMax().y) { return false; }
		if (max_.z < other.getMin().z || min_.z > other.getMax().z) { return false; }
		return true;
	}

	std::tuple<bool, glm::vec3, float> intersect(const Ray& ray)const
	{
		// p181
		const auto notIntersect = std::tuple<bool, glm::vec3, float>(false, glm::vec3(0.f), 0.f);
		float tmin = 0.f;
		float tmax = FLT_MAX;
		for (int i = 0; i < 3; i++)
		{
			if (glm::abs(ray.d_[i]) < FLT_EPSILON)
			{
				// 光線はスラブに対して平行。原点がスラブの中になければ交点無し。
				if (ray.p_[i] < min_[i] || ray.p_[i] > max_[i])
				{
					return notIntersect;
				}
			}
			else
			{
				float ood = 1.f / ray.d_[i];
				float t1 = (min_[i] - ray.p_[i]) * ood;
				float t2 = (max_[i] - ray.p_[i]) * ood;

				// t1が近い平面との交差、t2が遠い平面との交差になる
				if (t1 > t2){
					std::swap(t1, t2);
				}

				// スラブの交差している感覚との交差を計算
				if (t1 > tmin){
					tmin = t1;
				}
				if (t2 > tmax){
					tmax = t2;
				}

				if (tmin > tmax) {
					return notIntersect;
				}
			}
		}
		return std::tuple<bool, glm::vec3, float>(true, ray.p_ + ray.d_*tmin, tmin);
	}

	template<typename T>
	void calcAABB(const std::vector<T>& array)
	{
		min_ = glm::vec3(10e10f);
		max_ = glm::vec3(-10e10f);
		if (!array.empty())
		{
			min_ = array[0].xyz;
			max_ = array[0].xyz;

		}
		std::for_each(array.begin(), array.end(), [&](const T& v){
			if (min_.x > v.x) { min_.x = v.x; }
			else if (max_.x < v.x) { max_.x = v.x; }
			if (min_.y > v.y) { min_.y = v.y; }
			else if (max_.y < v.y) { max_.y = v.y; }
			if (min_.z > v.z) { min_.z = v.z; }
			else if (max_.z < v.z) { max_.z = v.z; }

		});
	}

	void calcAABB(const std::vector<glm::vec2>& array)
	{
		min_ = glm::vec3(10e10f);
		max_ = glm::vec3(-10e10f);
		if (!array.empty())
		{
			min_ = glm::vec3(array[0], 0.f);
			max_ = glm::vec3(array[0], 0.f);

		}

		std::for_each(array.begin(), array.end(), [&](const glm::vec2& v){
			if (min_.x > v.x) { min_.x = v.x; }
			else if (max_.x < v.x) { max_.x = v.x; }
			if (min_.y > v.y) { min_.y = v.y; }
			else if (max_.y < v.y) { max_.y = v.y; }
//			if (min_.z < v.z) { min_.z = v.z; }
//			else if (max_.z > v.z) { max_.z = v.z; }

		});
	}

	glm::vec3 calcCenter() const {
		return min_ + calcHalfSize();
	}

	glm::vec3 calcHalfSize() const {
		return (max_ - min_) * 0.5f;
	}

	const glm::vec3& getMin()const{ return min_; }
	const glm::vec3& getMax()const{ return max_; }
};
