#pragma once

#include "geocore.h"
#include "Engine/SimpleMath.h"

namespace geoutils
{
    vec4 create_vec4(vec3 const& v, float w = 1.f);
    matrix get_planematrix(vec3 const &translation, vec3 const &normal);
	std::vector<vec3> create_box(vec3 const &center, vec3 const& extents);
	std::vector<vec3> create_cube(vec3 const &center, float scale);
    bool are_equal(float const& l, float const& r);
    bool are_equal(vec3 const& l, vec3 const& r);

	template <typename T>
	T constexpr tolerance = T(1e-4f);

	// todo : error for non-numeric types including char, maybe use concepts
	template <typename T>
	struct invalid
	{
		constexpr operator T() const { return std::numeric_limits<T>::max(); }
	};

	template <typename T>
	bool constexpr isvalid(T const& val) { return std::numeric_limits<T>::max() - val >= tolerance<T>; }
}