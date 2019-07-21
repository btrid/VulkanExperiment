#ifndef Voronoi_H_
#define Voronoi_H_

#if defined(USE_Voronoi)


struct VoronoiCell
{
	i16vec2 point;
};

#define VoronoiVertex_MAX (10)
struct VoronoiPolygon
{
	int16_t vertex_index[VoronoiVertex_MAX];
	int num;
	i16vec4 minmax;
};

struct VoronoiVertex
{
	i16vec2 point;
	int16_t	cell[4];
};

layout(set=USE_Voronoi, binding=0, std430) restrict buffer rbVoronoiCellBuffer {
	VoronoiCell b_voronoi_cell[];
};
layout(set=USE_Voronoi, binding=1, std430) restrict buffer rbVoronoiPolygonBuffer {
	VoronoiPolygon b_voronoi_polygon[];
};
layout(set=USE_Voronoi, binding=2, std430) restrict buffer rbVoronoiBuffer {
	int16_t b_voronoi[];
};
layout(set=USE_Voronoi, binding=3, std430) restrict buffer rbVoronoiVertexCounter {
	uvec4 b_voronoi_vertex_counter;
};
layout(set=USE_Voronoi, binding=4, std430) restrict buffer rbVoronoiVertexBuffer {
	VoronoiVertex b_voronoi_vertex[];
};
layout(set=USE_Voronoi, binding=5, std430) restrict buffer rbVoronoiPathBuffer {
	int16_t b_voronoi_path[];
};

#endif

#endif