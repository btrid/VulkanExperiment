

struct ParticleInfo
{
	uint m_max_num;
	uint m_emit_max_num;
};
struct ParticleData
{
	vec4 m_pos;	//!< xyz:pos w:scale
	vec4 m_vel;	//!< xyz:dir w:not use
	uint m_type;
	uint m_flag;
	float m_life;
	uint _p;
};

struct DrawIndirectCommand
{
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};
