#pragma once

#include <array>
#include <unordered_map>
#include <btrlib/DefineMath.h>
#include <btrlib/Singleton.h>
#include <btrlib/cInput.h>
class cCamera
{
public:
	class sCamera;
	friend sCamera;
protected:
	cCamera()
		: m_distance(350.f)
		, m_up(0.f, 1.f, 0.f)
		, m_angle_y(0.f)
		, m_angle_x(0.f)
		, m_near(0.1f)
		, m_far(1000.f)
		, m_fov(glm::radians(40.f))
		, m_height(480)
		, m_width(640)
	{
	}

	~cCamera() {}
public:
	glm::vec3 m_position;
	glm::quat m_rotation;

	float m_distance;
	glm::vec3 m_target;
	glm::vec3 m_up;
	float m_angle_x;
	float m_angle_y;
	glm::mat4 m_projection;
	glm::mat4 m_view;
	float m_near;
	float m_far;
	float m_fov;
	int m_height;
	int m_width;

	float getAspect()const { return (float)m_width / m_height; }
	float getFov()const { return m_fov; }
	float getNear()const { return m_near; }
	float getFar()const { return m_far; }
	glm::vec3 getPosition()const { return m_position; }
	glm::vec3 getTarget()const { return m_target; }
	glm::vec3 getUp()const { return m_up; }
public:

	void control(const cInput& input, float deltaTime)
	{
		float distance = glm::length(m_target - m_position);
		{
			if (input.m_mouse.getWheel() != 0) {
				glm::vec3 dir = glm::normalize(m_position - m_target);
				distance += -input.m_mouse.getWheel() * distance / 10.f + 1.f;
				distance = glm::max(distance, 1.f);
				m_position = m_target + dir * distance;

			}
		}
// 		if (input.m_mouse.isHold(cMouse::BUTTON_LEFT))
// 		{
// 			// XZ•½–Ê‚ÌˆÚ“®
// 			auto f = glm::normalize(m_target - m_position);
// 			auto s = glm::normalize(glm::cross(m_up, f));
// 			auto move = input.m_mouse.getMove();
// 			m_position += (s*move.x + f*move.y) * distance / 100.f;
// 			m_target += (s*move.x + f*move.y) * distance / 100.f;
// 
// 		}
// 		else if (input.m_mouse.isHold(cMouse::BUTTON_MIDDLE))
// 		{
// 			// XY•½–Ê‚ÌˆÚ“®
// 			auto f = glm::normalize(m_target - m_position);
// 			auto s = glm::normalize(glm::cross(m_up, f));
// 			auto move = input.m_mouse.getMove();
// 			m_position += (s*move.x + m_up*move.y) * distance / 100.f;
// 			m_target += (s*move.x + m_up*move.y) * distance / 100.f;
// 		}
/*		else*/ if (input.m_mouse.isHold(cMouse::BUTTON_RIGHT))
		{
			// ‰ñ“]ˆÚ“®
			glm::vec3 f = glm::normalize(m_target - m_position);
			glm::vec3 s = glm::normalize(glm::cross(f, m_up));
			glm::vec3 u = glm::normalize(glm::cross(s, f));

			auto move = input.m_mouse.getMove();
			if (input.m_keyboard.isHold('A')) { move.y = 0; }
			glm::vec3 d = s*glm::radians(move.x / 2.f) + u*glm::radians(move.y / 2.f);

			m_position += d*distance;
			f = glm::normalize(m_position - m_target);
			m_position = m_target + f*distance;
			s = glm::normalize(glm::cross(f, m_up));
			m_up = glm::normalize(glm::cross(s, f));
		}

		m_view = glm::lookAt(m_position, m_target, m_up);

	}

public:
	class sCamera : public Singleton<sCamera>
	{
		friend Singleton<sCamera>;
		std::vector<cCamera*> m_camera;
	public:
		cCamera* create() {
			m_camera.emplace_back(new cCamera);
			return m_camera.back();
		}

		void destroy(cCamera* cam) {
			if (!cam) {
				return;
			}
			auto it = std::find(m_camera.begin(), m_camera.end(), cam);
			m_camera.erase(it);
			delete cam;
		}

		const std::vector<cCamera*>& getCameraList()const { return m_camera; }
	};

};

struct CameraGPU
{
	void setup(const cCamera& camera) 
	{
		mProjection = glm::perspective(camera.m_fov, camera.getAspect(), camera.m_near, camera.m_far);
		mView = glm::lookAt(camera.m_position, camera.m_target, camera.m_up);
	}
	glm::mat4 mProjection;
	glm::mat4 mView;
};

