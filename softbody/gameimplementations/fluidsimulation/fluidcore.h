#pragma once

#include "engine/stdx.h"
#include <array>
#include <vector>
#include <functional>

// todo : maybe fix this 0 based degree/dimension madness

namespace fluid
{
template<stdx::arithmetic_c t, uint d>
struct vec : public std::array<t, d + 1>
{
	static constexpr uint nd = d;
	constexpr vec operator-() const { return { stdx::unaryop(*this, stdx::uminus()) }; }
	constexpr vec operator+(vec r) const { return { stdx::binaryop(*this, r, std::plus<>()) }; }
	constexpr vec operator-(vec r) const { return { stdx::binaryop(*this, r, std::minus<>()) }; }
	constexpr vec operator/(t r) const { return { stdx::unaryop(*this, std::bind(std::multiplies<>(), std::placeholders::_1, 1 / r)) }; }

	constexpr operator t() const requires (d == 0) { return (*this)[0]; }
	constexpr t dot(vec r) const { return stdx::dot(*this, r); }
	constexpr vec clamp(t a, t b) const { return { stdx::clamp(*this, a, b) }; }
};

template<stdx::arithmetic_c t, uint d>
vec<t, d> operator*(vec<t, d> l, t r) { return { stdx::unaryop(l, std::bind(std::multiplies<>(), std::placeholders::_1, r)) }; }

template<stdx::arithmetic_c t, uint d>
vec<t, d> operator*(t l, vec<t, d> r) { return { stdx::unaryop(r, std::bind(std::multiplies<>(), std::placeholders::_1, l)) }; }

using vec1 = vec<float, 0>;
using vec2 = vec<float, 1>;

// d - dimension of field, vd - dimension of vector, l length of field(assuming hypercubic)
template <uint d, uint vd, uint l>
struct vecfield : public std::array<vec<float, vd>, stdx::pown(l, d + 1)> {};

template<uint vd, int l>
using vecfield2 = vecfield<1, vd, l>;

template<int l>
using vecfield21 = vecfield<1, 0, l>;

template<int l>
using vecfield22 = vecfield<1, 1, l>;

template <uint d, uint vd, uint l>
vecfield<d, vd, l> operator-(vecfield<d, vd, l> const& a) { return { stdx::binaryop(a, stdx::uminus()) }; }

template <uint d, uint vd, uint l>
vecfield<d, vd, l> operator+(vecfield<d, vd, l> const& a, vecfield<d, vd, l> const& b) { return { stdx::binaryop(a, b, std::plus<>()) }; }

template <uint d, uint vd, uint l>
vecfield<d, vd, l> operator-(vecfield<d, vd, l> const& a, vecfield<d, vd, l> const& b) { return { stdx::binaryop(a, b, std::minus<>()) }; }

// n is dimension of simulation(0-based), l is length of box/cube(number of cells)
template<uint n, uint l>
requires (n >= 0)
	struct fluidbox
{
	fluidbox(float _diff) : diff(_diff) {}
	static constexpr uint numcells = stdx::pown(l, n);
	using cubeidx = stdx::hypercubeidx<n>;

	float diff;

	vecfield21<l> d;
	vecfield22<l> v;
	vecfield22<l> v0;
	// diffusion coeffecient, density, velocity, oldvelocity

	void addvelocity(cubeidx const& cellidx, vec<float, n> vel)
	{
		// todo : provide shorthand assignment operators
		v[cubeidx::to1d<l - 1>(cellidx)] = { v[cubeidx::to1d<l - 1>(cellidx)] + vel };
	}

	void adddensity(cubeidx const& cellidx, float den)
	{
		// todo : provide shorthand assignment operators
		d[cubeidx::to1d<l - 1>(cellidx)] = { d[cubeidx::to1d<l - 1>(cellidx)] + den };
	}
};

// todo : figure out a way to generically handle arbitrary dimensions
// the problem right now is that we have to no way of generating nested loops based on template paramter
// a potential solution is to wirte loop templates that use recursion
// alternatvely a more desirable solution could be to iterate the 1d representation(since data in any dimension is just a 1D array) in single loop
// This might mean padding the vector field with additional cells outside boundary since we require neighbouring cells for solving poisson equations

template<uint vd>
vec<float, vd> bilerp(vec<float, vd> lt, vec<float, vd> rt, vec<float, vd> lb, vec<float, vd> rb, vec2 alpha)
{
	return (lt * (1.f - alpha[0]) + rt * alpha[0]) * (1.f - alpha[1]) + (lb * (1.f - alpha[0]) + rb * alpha[0]) * alpha[1];
}

// todo : use callable concept
template<uint vd, uint l>
vecfield2<vd, l> diffuse(vecfield2<vd, l> const& x, vecfield2<vd, l> const& b, float dt, float diff)
{
	using idx = stdx::hypercubeidx<1>;
	float const a = diff * dt;
	return jacobi2d(x, b, 4, a, 1 + 4 * a);
}

template<uint l>
vecfield21<l> divergence(vecfield22<l> const& f)
{
	using idx = stdx::hypercubeidx<1>;
	vecfield21<l> r;
	for (uint i(1); i < l - 1; ++i)
		for (uint j(1); j < l - 1; ++j)
			// todo : implement unary - for vector fields
			r[idx::to1d<l - 1>({ i, j })] = -0.5f * vec1{ f[idx::to1d<l - 1>({ i + 1, j })][0] - f[idx::to1d<l - 1>({ i - 1, j })][0] + f[idx::to1d<l - 1>({ i, j + 1 })][1] - f[idx::to1d<l - 1>({ i, j - 1 })][1] };

	return r;
}

template<uint l>
vecfield22<l> gradient(vecfield21<l> const& f)
{
	using idx = stdx::hypercubeidx<1>;
	vecfield22<l> r{};
	for (uint i(1); i < l - 1; ++i)
		for (uint j(1); j < l - 1; ++j)
			r[idx::to1d<l - 1>({ i, j })] = 0.5f * vec2{ f[idx::to1d<l - 1>({ i + 1, j })] - f[idx::to1d<l - 1>({ i - 1, j })], f[idx::to1d<l - 1>({ i, j + 1 })] - f[idx::to1d<l - 1>({ i, j - 1 })] };
	
	return r;
}

// jacobi iteration to solve poisson equations
template<uint vd, uint l>
vecfield2<vd, l> jacobi2d(vecfield2<vd, l> const& x, vecfield2<vd, l> const& b, uint niters, float alpha, float beta)
{
	using idx = stdx::hypercubeidx<1>;
	vecfield2<vd, l> r = x;
	auto const rcbeta = 1.f / beta;
	for (uint k(0); k < niters; ++k)
	{
		for (uint i(1); i < l - 1; ++i)
			for (uint j(1); j < l - 1; ++j)
				r[idx::to1d<l - 1>({ i, j })] = ((r[idx::to1d<l - 1>({ i - 1, j })] + r[idx::to1d<l - 1>({ i + 1, j })] + r[idx::to1d<l - 1>({ i, j - 1 })] +
					r[idx::to1d<l - 1>({ i, j + 1 })]) * alpha + b[idx::to1d<l - 1>({ i, j })]) * rcbeta;
	}
	return r;
}

template<uint vd, uint l>
vecfield2<vd, l> advect2d(vecfield2<vd, l> const& a, vecfield22<l> const& v, float dt)
{
	using idx = stdx::hypercubeidx<1>;
	vecfield2<vd, l> r{};
	for (uint i(1); i < l - 1; ++i)
		for (uint j(1); j < l - 1; ++j)
		{
			auto const cell = idx::to1d<l - 1>({ i, j });

			// find position at -dt
			vec2 const xy = (vec2{ static_cast<float>(i), static_cast<float>(j) } - v[cell] * dt).clamp(0.5f, l - 1.5f);

			// bilinearly interpolate the property across 4 neighbouring cells
			idx const lt2d = { static_cast<int>(xy[0]), static_cast<int>(xy[1]) };
			auto const lt = idx::to1d<l - 1>(lt2d);
			auto const rt = idx::to1d<l - 1>({ lt2d + idx{1, 0} });
			auto const lb = idx::to1d<l - 1>({ lt2d + idx{0, 1} });
			auto const rb = idx::to1d<l - 1>({ lt2d + idx{1, 1} });

			r[cell] = bilerp(a[lt], a[rt], a[lb], a[rb], {xy[0] - lt2d.coords[0], xy[1] - lt2d.coords[1] });
		}

	return r;
}

// todo : this can be done better by using vectors to set the boundary velocities/densities
// todo : this can be done even better by making this work for n-dimensional fluids, though it might be really difficult to do so

// todo : this requries 2 dimension vectors
template<uint vd, uint l>
void sbounds(vecfield2<vd, l>& v, float scale)
{
	using idx = stdx::hypercubeidx<1>;
	std::vector<idx> boundaries;
	for (uint i(1); i < l - 1; ++i)
	{
		boundaries.push_back({ 0, i });
		boundaries.push_back({ l - 1, i });
		boundaries.push_back({ i, 0 });
		boundaries.push_back({ i, l - 1 });
	}

	for (auto bcell : boundaries)
	{
		for (uint i(0); i <= vd; ++i)
		{
			if (bcell[0] == 0)
				v[idx::to1d<l - 1>(bcell)][i] = scale * v[idx::to1d<l - 1>(bcell + idx{1, 0})][i];

			if (bcell[0] == l - 1) 
				v[idx::to1d<l - 1>(bcell)][i] = scale * v[idx::to1d<l - 1>(bcell + idx{-1, 0})][i];

			if (bcell[1] == 0)
				v[idx::to1d<l - 1>(bcell)][i] = scale * v[idx::to1d<l - 1>(bcell + idx{0, 1})][i];
			
			if (bcell[1] == l - 1)
				v[idx::to1d<l - 1>(bcell)][i] = scale * v[idx::to1d<l - 1>(bcell + idx{0, -1})][i];
		}
	}

	// assign corner velocities as average of two neighbors
	v[idx::to1d<l - 1>({ 0, 0 })] = (v[idx::to1d<l - 1>({ 1, 0 })] + v[idx::to1d<l - 1>({ 0, 1 })]) / 2.f;
	v[idx::to1d<l - 1>({ 0, l - 1 })] = (v[idx::to1d<l - 1>({ 1, l - 1 })] + v[idx::to1d<l - 1>({ 0, l - 2 })]) / 2.f;
	v[idx::to1d<l - 1>({ l - 1, 0 })] = (v[idx::to1d<l - 1>({ l - 2, 0 })] + v[idx::to1d<l - 1>({ l - 1, 1 })]) / 2.f;
	v[idx::to1d<l - 1>({ l - 1, l - 1 })] = (v[idx::to1d<l - 1>({ l - 2, l - 1 })] + v[idx::to1d<l - 1>({ l - 1, l - 2 })]) / 2.f;
}
}