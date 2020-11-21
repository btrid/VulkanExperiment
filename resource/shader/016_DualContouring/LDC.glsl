#ifndef LDC_H_
#define LDC_H_

#extension GL_EXT_ray_tracing : enable

layout(set=0,binding=0) uniform accelerationStructureEXT topLevelAS;
layout(set=1,binding=1) buffer LDCCounter { uint b_ldc_counter; };
layout(set=1,binding=2) buffer LDCArea { uvec2 b_ldc_area[]; };
layout(set=1,binding=3) buffer LDCRange { vec2 b_ldc_range[]; };

layout(location = 0) rayPayloadEXT bool IsEnd;

#endif // LDC_H_

