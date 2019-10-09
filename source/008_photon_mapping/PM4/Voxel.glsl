

//#define getCellSize0() BRICK_CELL_SIZE
#define getVoxelCellSize() BRICK_SUB_CELL_SIZE
//#define getIndexVoxel(pos) ivec3((pos - BRICK_AREA_MIN) / BRICK_CELL_SIZE)
#define getVoxelIndex(pos) ivec3((pos - BRICK_AREA_MIN) / BRICK_SUB_CELL_SIZE)
#define getTiledVoxelIndex(index1) \
	convert3DTo1D(index1 / BRICK_SCALE1, ivec3(BRICK_SIZE))*BRICK_SCALE1*BRICK_SCALE1*BRICK_SCALE1 \
	+ convert3DTo1D(index1 - ((index1 / BRICK_SCALE1)*BRICK_SCALE1), ivec3(BRICK_SCALE1))

bool isInRange(in ivec3 index, in ivec3 size)
{
	return all(lessThan(index, size)) && all(greaterThanEqual(index, ivec3(0)));
}

