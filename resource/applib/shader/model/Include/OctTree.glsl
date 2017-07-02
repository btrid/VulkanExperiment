
#define OCTREE_SIZE 10

uniform vec3 OctreeMax;
uniform vec3 OctreeMin;
uniform vec3 OctreeCellSize;
uniform int OctreeSize;
uniform uint OctreeDepth;
uniform int OctreeCellNum[OCTREE_SIZE];

//uniform int OctreeOffset[OCTREE_SIZE];
uniform int OctreeOffset[9] = {1, 8, 64, 512, 4096, 32768, 262144, 2097152, 16777216};

uint calcDepth(uint key)
{
	uint i = 0;
	for (; i<OCTREE_SIZE; i++) {
		if (key < OctreeCellNum[i]){
			break;
		}
	}
	return i;
	
}

vec3 getCellSize(uint depth)
{
	if(depth > OctreeDepth) { return vec3(0.); }

	uint diff = OctreeDepth - depth;
	vec3 size = OctreeCellSize;
	for (uint i = 0; i < OCTREE_SIZE; i++)
	{
		if(diff <= 0 ){
			break;
		}
		size *= 2.;
		diff--;
	}
	return size;
}

vec3 calcPositionByMortonKey(uint key)
{
	vec3 pos = vec3(0.);
	vec3 size = vec3(OctreeMax - OctreeMin);
	uint depth = calcDepth(key);
	for(int i = 0; i<OCTREE_SIZE; i++)
	{
		if(depth == 0) {
			break;
		}

		uint flag = 7 << ((depth-1)*3);
		uint bit = (flag & (key-OctreeCellNum[depth-1])) >> ((depth-1) * 3);
		size *= 0.5;
		if ((bit & 1) != 0) {
			pos.x += size.x;
		}
		if ((bit & 2) != 0) {
			pos.y += size.y;
		}
		if ((bit & 4) != 0) {
			pos.z += size.z;
		}
		depth--;
	}
	return pos + OctreeMin + size*0.5;
}


uint part1By2(uint n)
{
	n = (n ^ (n << 16)) & 0xff0000ff;
	n = (n ^ (n << 8)) & 0x0300f00f;
	n = (n ^ (n << 4)) & 0x030c30c3;
	n = (n ^ (n << 2)) & 0x09249249;
	return n;
}

uint morton3(uvec3 xyz)
{
	return (part1By2(xyz.z) << 2) | (part1By2(xyz.y) << 1) | part1By2(xyz.x);
}


int getMortonNumber(in vec3 pos, float radius)
{
	vec3 min = (pos - vec3(radius*0.5) - OctreeMin) / OctreeCellSize;
	uint minMorton = morton3(uvec3(min));
	vec3 max = (pos + vec3(radius*0.5) - OctreeMin) / OctreeCellSize;
	uint maxMorton = morton3(uvec3(max));

	uint difference = maxMorton ^ minMorton;
	uint depth = 0;
	for (uint i = 0; i<OCTREE_SIZE; i++)
	{
		uint check = (difference >> (i * 3)) & 0x7;
		if (check != 0){
			depth = i+1;
		}
	}

	uint spaceNum = maxMorton >> (depth * 3);
	uint offset = (OctreeOffset[OctreeDepth - depth]-1) / 7;
	spaceNum += offset;

	if (spaceNum >= OctreeSize) {
		return -1;
	}

	return int(spaceNum);

}

