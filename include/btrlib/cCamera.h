#pragma once

#include <array>
#include <unordered_map>
#include <btrlib/DefineMath.h>
#include <btrlib/Singleton.h>
#include <btrlib/Shape.h>
#include <btrlib/cInput.h>
class cCamera
{
	struct Data
	{
		vec3 m_position;
		quat m_rotation;

		float m_distance;
		vec3 m_target;
		vec3 m_up;
		float m_angle_x;
		float m_angle_y;
		mat4 m_projection;
		mat4 m_view;
		float m_near;
		float m_far;
		float m_fov;
		int m_height;
		int m_width;

		float getAspect()const { return (float)m_width / m_height; }
	};

	Data m_data;
	Data m_render;
public:
	class sCamera;
	friend sCamera;
protected:
	cCamera()
	{
		m_data.m_distance = 350.f;
		m_data.m_up = vec3(0.f, 1.f, 0.f);
		m_data.m_angle_y = 0.f;
		m_data.m_angle_x = 0.f;
		m_data.m_near = 0.1f;
		m_data.m_far = 1000.f;
		m_data.m_fov = glm::radians(40.f);
		m_data.m_height = 480;
		m_data.m_width = 640;

		m_render = m_data;
	}

	~cCamera() {}
public:
	Data& getData() { return m_data; }
	const Data& getData()const { return m_data; }
	float getAspect()const { return m_data.getAspect(); }
	float getFov()const { return m_data.m_fov; }
	float getNear()const { return m_data.m_near; }
	float getFar()const { return m_data.m_far; }
	vec3 getPosition()const { return m_data.m_position; }
	vec3 getTarget()const { return m_data.m_target; }
	vec3 getUp()const { return m_data.m_up; }

	Data& getRenderData() { return m_render; }
	const Data& getRenderData()const { return m_render; }
public:

	void control(const cInput& input, float deltaTime)
	{
		float distance = length(m_data.m_target - m_data.m_position);
		{
			if (input.m_mouse.getWheel() != 0) 
			{
				vec3 dir = normalize(m_data.m_position - m_data.m_target);
				distance += -input.m_mouse.getWheel() * distance * deltaTime*5.f;
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
			m_data.m_position += (s*move.x + f*move.y) * distance * deltaTime;
			m_data.m_target += (s*move.x + f*move.y) * distance * deltaTime;

		}else if (input.m_mouse.isHold(cMouse::BUTTON_LEFT))
		{
			// XY•½–Ê‚ÌˆÚ“®
			auto move = vec2(input.m_mouse.getMove());
			m_data.m_position += (s*move.x + m_data.m_up*move.y) * distance * deltaTime;
			m_data.m_target += (s*move.x + m_data.m_up*move.y) * distance * deltaTime;
		}
		else if (input.m_mouse.isHold(cMouse::BUTTON_RIGHT))
		{
			// ‰ñ“]ˆÚ“®
			vec3 u = m_data.m_up;

			auto move = vec2(input.m_mouse.getMove());
			if (!glm::epsilonEqual(dot(move, move), 0.f, FLT_EPSILON))
			{
				quat rot = glm::angleAxis(glm::radians(1.0f), normalize(move.y*s + move.x*u));
				f = normalize(rot * f);
				m_data.m_target = m_data.m_position + f*distance;
				s = normalize(cross(m_data.m_up, f));
			}
		}

		m_data.m_view = lookAt(m_data.m_position, m_data.m_target, m_data.m_up);

	}

public:
	class sCamera : public Singleton<sCamera>
	{
		friend Singleton<sCamera>;
		std::vector<std::shared_ptr<cCamera>> m_camera;
	public:
		std::shared_ptr<cCamera> create() {
			struct C : cCamera {};
			auto camera = std::make_shared<C>();
			m_camera.emplace_back(camera);
			return camera;
		}

		void destroy(const std::shared_ptr<cCamera>& cam) 
		{
			if (!cam) {
				return;
			}
			auto it = std::find(m_camera.begin(), m_camera.end(), cam);
			if (it != m_camera.end())
			{
				m_camera.erase(it);
			}
		}

		const std::vector<std::shared_ptr<cCamera>>& getCameraList()const { return m_camera; }
	};

};

struct FrustomPoint
{
	vec4 ltn;	//!< nearTopLeft
	vec4 rtn;
	vec4 lbn;
	vec4 rbn;
	vec4 ltf;	//!< nearTopLeft
	vec4 rtf;
	vec4 lbf;
	vec4 rbf;
};

class Frustom {

public:
	vec3 ltn_;	//!< nearTopLeft
	vec3 rtn_;
	vec3 lbn_;
	vec3 rbn_;
	vec3 ltf_;	//!< nearTopLeft
	vec3 rtf_;
	vec3 lbf_;
	vec3 rbf_;

	float nearD_, farD_, aspect_, fov_;
	float nw_, nh_, fw_, fh_;

	enum {
		PLANE_TOP,
		PLANE_BOTTOM,
		PLANE_RIGHT,
		PLANE_LEFT,
		PLANE_NEAR,
		PLANE_FAR,
		PLANE_NUM,
	};
	std::array<Plane, 6> plane_;
public:

	std::array<Plane, 6>& getPlane() { return plane_; }
	Frustom()
	{}

	void setup(const cCamera& camera)
	{
		const auto& cam = camera.getRenderData();
		setProjection(cam.m_fov, cam.getAspect(), cam.m_near, cam.m_far);
		setView(cam.m_position, cam.m_target, cam.m_up);
	}
	void setProjection(float fov, float aspect, float nearD, float farD)
	{
		// store the information
		aspect_ = aspect;
		fov_ = fov;
		nearD_ = nearD;
		farD_ = farD;

		// compute width and height of the near and far plane sections
		float t = tan(fov * 0.5f);
		nh_ = nearD * t;
		nw_ = nh_ * aspect;
		fh_ = farD  * t;
		fw_ = fh_ * aspect;
	}
	void setView(const vec3 &p, const vec3 &l, const vec3 &u) 
	{

		vec3 Z = normalize(p - l);
		vec3 X = normalize(cross(u, Z));
		vec3 Y = normalize(cross(Z, X));

		vec3 nc = p - Z * nearD_;
		vec3 fc = p - Z * farD_;

		// compute the 4 corners of the frustum on the near plane
		ltn_ = nc + Y * nh_ - X * nw_;
		rtn_ = nc + Y * nh_ + X * nw_;
		lbn_ = nc - Y * nh_ - X * nw_;
		rbn_ = nc - Y * nh_ + X * nw_;

		// compute the 4 corners of the frustum on the far plane
		ltf_ = fc + Y * fh_ - X * fw_;
		rtf_ = fc + Y * fh_ + X * fw_;
		lbf_ = fc - Y * fh_ - X * fw_;
		rbf_ = fc - Y * fh_ + X * fw_;

		// compute the six planes
		// the function set3Points assumes that the points
		// are given in counter clockwise order
		plane_[PLANE_TOP] = Plane(rtn_, ltn_, ltf_);
		plane_[PLANE_BOTTOM] = Plane(lbn_, rbn_, rbf_);
		plane_[PLANE_LEFT] = Plane(ltn_, lbn_, lbf_);
		plane_[PLANE_RIGHT] = Plane(rbn_, rtn_, rbf_);
		plane_[PLANE_NEAR] = Plane(ltn_, rtn_, rbn_);
		plane_[PLANE_FAR] = Plane(rtf_, ltf_, lbf_);

	}

	bool isInFrustom(const vec3& p) {
		for (int i = 0; i < PLANE_NUM; i++) {
			if (plane_[i].getDistance(p) < 0.f) {
				return false;
			}
		}

		return true;
	}
	bool isInFrustom(const vec3& p, float radius)
	{
		for (int i = 0; i < PLANE_NUM; i++) {
			if (plane_[i].getDistance(p) < radius) {
				return false;
			}
		}
		return true;
	}
};

struct CameraGPU
{
	void setup(const cCamera& camera)
	{
		const auto& cam = camera.getRenderData();
#if BTR_USE_REVERSED_Z
		u_projection = glm::perspective(cam.m_fov, cam.getAspect(), cam.m_far, cam.m_near);
#else
		u_projection = glm::perspective(cam.m_fov, cam.getAspect(), cam.m_near, cam.m_far);
#endif
		u_view = lookAt(cam.m_position, cam.m_target, cam.m_up);
		u_eye = vec4(cam.m_position, 0.f);
		u_target = vec4(cam.m_target, 0.f);
		u_up = vec4(cam.m_up, 0.f);
		u_aspect = cam.getAspect();
		u_fov_y = cam.m_fov;
		u_near = cam.m_near;
		u_far = cam.m_far;

		Frustom f;
		f.setup(camera);
		m_plane = f.getPlane();

	}
	mat4 u_projection;
	mat4 u_view;
	vec4 u_eye;
	vec4 u_target;
	vec4 u_up;
	float u_aspect;
	float u_fov_y;
	float u_near;
	float u_far;

	std::array<Plane, 6> m_plane;
};
struct CameraFrustomGPU
{
	void setup(const cCamera& camera)
	{
		Frustom f;
		f.setup(camera);

		m_frustom_point.ltn = vec4(f.ltn_, 1.f);
		m_frustom_point.lbn = vec4(f.lbn_, 1.f);
		m_frustom_point.rtn = vec4(f.rtn_, 1.f);
		m_frustom_point.rbn = vec4(f.rbn_, 1.f);
		m_frustom_point.ltf = vec4(f.ltf_, 1.f);
		m_frustom_point.lbf = vec4(f.lbf_, 1.f);
		m_frustom_point.rtf = vec4(f.rtf_, 1.f);
		m_frustom_point.rbf = vec4(f.rbf_, 1.f);
	}
	FrustomPoint m_frustom_point;
};


