#pragma once

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/json.hpp>

namespace glm
{
template<class Archive>
void serialize(Archive & archive, vec2 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
}

template<class Archive>
void serialize(Archive & archive, vec3 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
}

template<class Archive>
void serialize(Archive & archive, vec4 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w));
}

template<class Archive>
void serialize(Archive & archive, uvec2 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
}

template<class Archive>
void serialize(Archive & archive, uvec3 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
}

template<class Archive>
void serialize(Archive & archive, uvec4 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w));
}
template<class Archive>
void serialize(Archive & archive, ivec2 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y));
}

template<class Archive>
void serialize(Archive & archive, ivec3 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z));
}

template<class Archive>
void serialize(Archive & archive, ivec4 &v)
{
	archive(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w));
}

}