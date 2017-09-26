
#pragma optionNV (unroll none)
#define assert(expression) while(!(expression)) {;}

struct QuadTreeCell
{
	int MortonNumber;
	int Tail;
	int NodeNum;
	uint isLock;
};

struct QuadTreeNode
{
	int MortonNumber;
	int NodeIndex;
	int EnemyID;	//!< 敵識別用Index
	int Next;
	int Prev;
	int _p0;
	int _p1;
	int _p2;
//	uint isLock;
//	int padding_;
//	int padding1_;
};

uniform int QuadTreePowArray[11] = {1, 4, 16, 64, 256, 1024, 4096, 16384, 65536, 262144, 1048576};

layout(std140, binding = 2) coherent restrict buffer  QuadTreeCellList
{
	QuadTreeCell CellList[];
};

layout(std140, binding = 3) coherent restrict buffer  QuadTreeNodeList
{
	QuadTreeNode NodeList[];
};

uniform vec2 MaxSize;
uniform vec2 MinSize;
uniform vec2 MinCellSize;
uniform int CellNum;
uniform int Depth;


int getCellListSize(int depth)
{
	return CellNum;
}

uint part1By1(uint n)
{
	n = (n | (n << 8)) & 0x00ff00ff;
	n = (n | (n << 4)) & 0x0f0f0f0f;
	n = (n | (n << 2)) & 0x33333333;
	return (n | (n << 1)) & 0x55555555;
}

uint morton2(in uvec2 xy)
{
	return (part1By1(xy.y) << 1) | part1By1(xy.x);
}

int getMortonNumber(in vec2 pos, float radius)
{
	vec2 min = (pos - vec2(radius*0.5) - MinSize) / MinCellSize;
	uint minMorton = morton2(uvec2(min));
	vec2 max = (pos + vec2(radius*0.5) - MinSize) / MinCellSize;
	uint maxMorton = morton2(uvec2(max));

	uint def = minMorton ^ maxMorton;
	int depth = 0;
	for(int i=0; i<Depth; i++)
	{
		uint check = (def>>(i*2)) & 0x3;
		if( check != 0 )
			depth = i+1;
	}
	uint spaceNum = maxMorton>>(depth*2);
	int offset = (QuadTreePowArray[Depth - depth] - 1) / 3;
	spaceNum += offset;

	if (spaceNum >= CellNum) {
		return -1;
	}

	return int(spaceNum);

}

void removeTreeList(int nodeIndex)
{
	QuadTreeNode node = NodeList[nodeIndex];
	// 登録されていない
	if(node.MortonNumber <= -1){
		return;
	}

	int key = node.MortonNumber;
	bool done = false;
	while (!done) 
	{
		if(done = (atomicExchange(CellList[key].isLock,1u)==0u))
		{
			QuadTreeCell cell = CellList[key];
			if (NodeList[nodeIndex].Next != -1) {
				NodeList[NodeList[nodeIndex].Next].Prev = NodeList[nodeIndex].Prev;
			}
			if (NodeList[nodeIndex].Prev != -1) {
				NodeList[NodeList[nodeIndex].Prev].Next, NodeList[nodeIndex].Next;
			}

			if (cell.Tail == nodeIndex) {
				CellList[key].Tail = NodeList[nodeIndex].Next;
			}
			NodeList[nodeIndex].Prev = -1;
			NodeList[nodeIndex].Next = -1;
			NodeList[nodeIndex].MortonNumber = -1;
			atomicExchange(CellList[key].isLock,0u);
		}
	}
}
		
void pushTreeList(in vec2 pos, in float radius, in int nodeIndex, int enemyID)
{
	NodeList[nodeIndex].EnemyID = enemyID;

	int mortonNumber = getMortonNumber(pos, 0.);
	if (NodeList[nodeIndex].MortonNumber == mortonNumber) {
		return;
	}

	if (NodeList[nodeIndex].MortonNumber >= 0) {
		removeTreeList(nodeIndex);
	}
	
	if(mortonNumber == -1) {
		return ;
	}
	bool done = false;
	while (!done) 
	{
		if(done = (atomicExchange(CellList[mortonNumber].isLock, 1u) == 0u))
		{
			if (CellList[mortonNumber].Tail != -1) {
				NodeList[CellList[mortonNumber].Tail].Prev = nodeIndex;
			}
			
			NodeList[nodeIndex].Prev = -1;
			NodeList[nodeIndex].Next = CellList[mortonNumber].Tail;
			NodeList[nodeIndex].MortonNumber = mortonNumber;
			CellList[mortonNumber].Tail = nodeIndex;
			atomicExchange(CellList[mortonNumber].isLock, 0u);
		}
	}	
}
