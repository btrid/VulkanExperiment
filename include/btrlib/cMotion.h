#pragma once
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <btrlib/DefineMath.h>
#include <btrlib/sGlobal.h>
struct cMotion
{
	struct VecKey
	{
		float m_time;
		glm::vec3 m_data;
	};
	struct QuatKey
	{
		float m_time;
		glm::quat m_data;
	};
	struct NodeMotion
	{
		uint32_t m_node_index;
		std::string m_nodename;
		std::vector<VecKey> m_translate;
		std::vector<VecKey> m_scale;
		std::vector<QuatKey> m_rotate;
	};
	std::string m_name;
	float m_duration;
	float m_ticks_per_second;
	uint32_t m_node_num;
	std::vector<NodeMotion> m_data;
};
struct cAnimation
{
	std::vector<std::shared_ptr<cMotion>> m_motion;
};

