#pragma once

#include <array>
#include <unordered_map>
#include <btrlib/DefineMath.h>
#include <btrlib/Singleton.h>
class cCamera
{
public:
	class sCamera;
	friend sCamera;
protected:
	cCamera()
		: mDistance(350.f)
		, mUp(0.f, 1.f, 0.f)
		, mAngleY(0.f)
		, mAngleX(0.f)
		, mNear(0.1f)
		, mFar(1000.f)
		, mFov(glm::radians(40.f))
		, mHeight(480)
		, mWidth(640)
	{
	}

	~cCamera() {}
public:
	glm::vec3 mPosition;
	glm::quat mRotation;

	float mDistance;
	glm::vec3 mTarget;
	glm::vec3 mUp;
	float mAngleX;
	float mAngleY;
	glm::mat4 mProjection;
	glm::mat4 mView;
	float mNear;
	float mFar;
	float mFov;
	int mHeight;
	int mWidth;

	float getAspect()const { return (float)mWidth / mHeight; }
public:

	void control(float deltaTime)
	{
		// todo mouse
/*		float distance = glm::length(target_ - position_);
		{
			if (Mouse::Order()->getWheelScroll().y != 0.f) {
				glm::vec3 dir = glm::normalize(position_ - target_);
				distance += -Mouse::Order()->getWheelScroll().y * distance / 10.f + 1.f;
				distance = glm::max(distance, 1.f);
				position_ = target_ + dir * distance;

			}
		}
		if (Mouse::Order()->isHold(MouseButton::LEFT))
		{
			// XZ•½–Ê‚ÌˆÚ“®
			auto f = glm::normalize(target_ - position_);
			auto s = glm::normalize(glm::cross(up_, f));
			auto move = Mouse::Order()->getMove();
			position_ += (s*move.x + f*move.y) * distance / 100.f;
			target_ += (s*move.x + f*move.y) * distance / 100.f;

		}
		else if (Mouse::Order()->isHold(MouseButton::MIDDLE))
		{
			// XY•½–Ê‚ÌˆÚ“®
			auto f = glm::normalize(target_ - position_);
			auto s = glm::normalize(glm::cross(up_, f));
			auto move = Mouse::Order()->getMove();
			position_ += (s*move.x + up_*move.y) * distance / 100.f;
			target_ += (s*move.x + up_*move.y) * distance / 100.f;
		}
		else if (Mouse::Order()->isHold(MouseButton::RIGHT))
		{
			// ‰ñ“]ˆÚ“®
			glm::vec3 f = glm::normalize(target_ - position_);
			glm::vec3 s = glm::normalize(glm::cross(f, up_));
			glm::vec3 u = glm::normalize(glm::cross(s, f));

			auto move = Mouse::Order()->getMove();
			if (KeyBoard::Order()->isHold(KeyBoard::CTRL_LEFT)) { move.y = 0.f; }
			glm::vec3 d = s*glm::radians(move.x / 2.f) + u*glm::radians(move.y / 2.f);

			position_ += d*distance;
			f = glm::normalize(position_ - target_);
			position_ = target_ + f*distance;
			s = glm::normalize(glm::cross(f, up_));
			up_ = glm::normalize(glm::cross(s, f));
		}

		view_ = glm::lookAt(position_, target_, up_);
*/
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
		mView = glm::lookAt(camera.mPosition, camera.mTarget, camera.mUp);
		mProjection = glm::perspective(camera.mFov, camera.getAspect(), camera.mNear, camera.mFar);
	}
	glm::mat4 mProjection;
	glm::mat4 mView;
};

