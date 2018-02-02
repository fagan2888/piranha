/* Copyright 2009-2017 Francesco Biscani (bluescarni@gmail.com)

This file is part of the Piranha library.

The Piranha library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 3 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The Piranha library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the Piranha library.  If not,
see https://www.gnu.org/licenses/. */

#ifndef PIRANHA_MATH_IS_ZERO_HPP
#define PIRANHA_MATH_IS_ZERO_HPP

#include <type_traits>
#include <utility>

#include <piranha/config.hpp>
#include <piranha/type_traits.hpp>

namespace piranha
{

namespace math
{

inline namespace impl
{

// Enabler condition for the default math::is_zero() implementation.
// NOTE: the equality comparable requirement already implies that the return type of
// the comparison must be convertible to bool.
template <typename T>
using has_default_math_is_zero = conjunction<std::is_constructible<T, const int &>, is_equality_comparable<T>>;
}

// Default functor for the implementation of piranha::math::is_zero().
template <typename T, typename = void>
class is_zero_impl
{
public:
    template <typename U, enable_if_t<has_default_math_is_zero<U>::value, int> = 0>
    bool operator()(const U &x) const
    {
        return x == U(0);
    }
};

inline namespace impl
{

// Enabler+return type for math::is_zero().
template <typename T>
using math_is_zero_t
    = enable_if_t<std::is_convertible<decltype(is_zero_impl<T>{}(std::declval<const T &>())), bool>::value, bool>;
}

template <typename T>
inline math_is_zero_t<T> is_zero(const T &x)
{
    return is_zero_impl<T>{}(x);
}

// Specialisation of the piranha::math::is_zero() functor for C++ complex floating-point types.
#if defined(PIRANHA_HAVE_CONCEPTS)
template <CppComplex T>
class is_zero_impl<T>
#else
template <typename T>
class is_zero_impl<T, enable_if_t<is_cpp_complex<T>::value>>
#endif
{
public:
    bool operator()(const T &c) const
    {
        return math::is_zero(c.real()) && math::is_zero(c.imag());
    }
};
}

inline namespace impl
{

template <typename T>
using is_zero_t = decltype(math::is_zero(std::declval<const T &>()));
}

// Type trait to detect the presence of the piranha::math::is_zero() function.
template <typename T>
using is_is_zero_type = is_detected<is_zero_t, T>;

#if defined(PIRANHA_HAVE_CONCEPTS)

template <typename T>
concept bool IsZeroType = is_is_zero_type<T>::value;

#endif
}

#endif
