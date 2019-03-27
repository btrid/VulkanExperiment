#pragma once
#include <memory>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Context.h>
#include <applib/App.h>
#include <applib/GraphicsResource.h>
#include <013_2DGI/GI2D/GI2DContext.h>


struct PhysicsWorld;

struct GI2DRigidbody
{
	struct Rigidbody
	{
		int32_t pnum;
		int32_t solver_count;
		float inertia;
		float mass;

		vec2 center;
		vec2 size;

		vec2 pos;
		vec2 pos_old;
		vec2 vel;
		vec2 vel_old;

		ivec2 pos_work;
		ivec2 vel_work;

		float angle;
		float angle_vel;
		int32_t angle_vel_work;
		uint32_t dist;

		ivec2 damping_work;
		ivec2 pos_bit_size;

		uint32_t id;
		uint32_t contact_count;
		uint32_t _p2;
		uint32_t _p3;

		uint32_t contact_list[16];

	};

	struct rbParticle
	{
		vec2 pos;
		vec2 pos_predict;
		vec2 pos_old;
		vec2 local_pos;
		vec2 vel;
		uint contact_index;
		uint is_contact;
	};

	struct rbContact
	{
		uint32_t contact_id;
	};


	GI2DRigidbody(const std::shared_ptr<PhysicsWorld>& world, const uvec4& box);
	btr::BufferMemoryEx<Rigidbody> b_rigidbody;
	btr::BufferMemoryEx<vec2> b_relative_pos;
	btr::BufferMemoryEx<rbParticle> b_rbparticle;
	btr::BufferMemoryEx<uint64_t> b_rbpos_bit;
	btr::BufferMemoryEx<uint32_t> b_contact_bit;
	btr::BufferMemoryEx<float> b_sdf;
	int32_t m_particle_num;
	vk::UniqueDescriptorSet m_descriptor_set;

};
