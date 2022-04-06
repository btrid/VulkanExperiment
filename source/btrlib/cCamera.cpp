#include <btrlib/cCamera.h>

void cCamera::control(const cInput& input, float deltaTime)
{
	float distance = length(m_data.m_target - m_data.m_position);
	{
		if (input.m_mouse.getWheel() != 0)
		{
			vec3 dir = normalize(m_data.m_position - m_data.m_target);
			distance += -input.m_mouse.getWheel() * distance * deltaTime * 5.f;
			distance = glm::max(distance, 1.f);
			m_data.m_position = m_data.m_target + dir * distance;

		}
	}
	auto f = normalize(m_data.m_target - m_data.m_position);
	auto s = normalize(cross(f, m_data.m_up));
	if (input.m_mouse.isHold(cMouse::BUTTON_MIDDLE))
	{
		// XZ•½–Ê‚ÌˆÚ“®
		auto move = vec2(input.m_mouse.getMove());
		m_data.m_position += (s * move.x + f * move.y) * distance * deltaTime;
		m_data.m_target += (s * move.x + f * move.y) * distance * deltaTime;

	}
	else if (input.m_mouse.isHold(cMouse::BUTTON_LEFT))
	{
		// XY•½–Ê‚ÌˆÚ“®
		auto move = vec2(input.m_mouse.getMove());
		m_data.m_position += (s * move.x + m_data.m_up * move.y) * distance * deltaTime;
		m_data.m_target += (s * move.x + m_data.m_up * move.y) * distance * deltaTime;
	}
	else if (input.m_mouse.isHold(cMouse::BUTTON_RIGHT))
	{
		// ‰ñ“]ˆÚ“®
//		vec3 u = m_data.m_up;
//		auto f = normalize(m_data.m_target - m_data.m_position);
		auto u = normalize(cross(f, s));

		auto move = vec2(input.m_mouse.getMove());
		if (!glm::epsilonEqual(dot(move, move), 0.f, FLT_EPSILON))
		{
			f = glm::quat(glm::radians(move.y), s) * glm::quat(glm::radians(move.x), u) * f; ;

//			f = normalize(rot * f);
			m_data.m_target = m_data.m_position + f * distance;
//			u = normalize(cross(s, f));
//			m_data.m_up = u;
		}
	}

	m_data.m_view = lookAt(m_data.m_position, m_data.m_target, m_data.m_up);
}
