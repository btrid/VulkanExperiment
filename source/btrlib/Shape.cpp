#include <btrlib/Shape.h>

bool Plane::isHit(const AABB& a)const
{
	glm::vec3 c = (a.max_ + a.min_) * 0.5f;
	glm::vec3 e = a.max_ - c;

	float r = glm::dot(e, glm::abs(normal_));
	float d = glm::dot(normal_, c) - dot_;
	return glm::abs(d) <= r;
}
