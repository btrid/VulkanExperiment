// helper
uvec3 convert1DTo3D(in uint i, in uvec3 size)
{
	uint x = i % size.x;
    uint y = (i / size.x) % size.y;
    uint z = i / (size.y * size.x);

    return uvec3(x, y, z);
}

uint convert3DTo1D(in uvec3 i, in uvec3 size){
	return i.x + i.y*size.x + i.z*size.x*size.y;
}


uvec2 convert1DTo2D(in uint i, in uvec2 size)
{
	uint x = i % size.x;
    uint y = i / size.x;

    return uvec2(x, y);
}
uint convert2DTo1D(in uvec2 i, in uvec2 size){
	return i.x + i.y*size.x;
}
