#pragma once

#include <vector>
#include <btrlib/Define.h>
#include <btrlib/DefineMath.h>
#include <btrlib/Shape.h>
#include <btrlib/Context.h>
#include <btrlib/AllocatedMemory.h>

struct FluidContext
{
	vk::UniqueDescriptorSetLayout m_DSL;
	vk::UniqueDescriptorSet m_DS;

	FluidContext(btr::Context& ctx)
	{
		// descriptor set layout
		{
			auto stage = vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment;
			vk::DescriptorSetLayoutBinding binding[] = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, stage),
				vk::DescriptorSetLayoutBinding(10, vk::DescriptorType::eStorageBuffer, 1, stage),
			};
			vk::DescriptorSetLayoutCreateInfo desc_layout_info;
			desc_layout_info.setBindingCount(array_length(binding));
			desc_layout_info.setPBindings(binding);
			m_DSL = ctx.m_device.createDescriptorSetLayoutUnique(desc_layout_info);
		}
	}
};
struct FluidData
{
	int PNum;
	std::vector<vec3> Acc, Pos, Vel;
	std::vector<float> Prs;
	std::vector<int> PType;

	float viscosity = 0.000001f; //!< 粘度

	Triangle triangle;
	std::vector<int> wallenable;

	struct Constant
	{
		float GridCellSize; //GridCellの大きさ(バケット1辺の長さ)
		float GridCellSizeInv;
		int GridCellTotal;
		float p1;
		ivec3 GridCellNum;
		float p2;
		vec3 GridMin;
		float p3;
		vec3 GridMax;
		float p4;
	};

	Constant m_constant;
	btr::BufferMemoryEx<Constant> u_constant;
	btr::BufferMemoryEx<int32_t> b_WallEnable;
	vk::UniqueDescriptorSet m_DS;

};
void init(FluidData& cFluid); 
void run(FluidData& cFluid);
void gui(FluidData& cFluid);

