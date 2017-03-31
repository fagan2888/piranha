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

#ifndef PIRANHA_KRONECKER_MONOMIAL_HPP
#define PIRANHA_KRONECKER_MONOMIAL_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include <piranha/config.hpp>
#include <piranha/detail/cf_mult_impl.hpp>
#include <piranha/detail/km_commons.hpp>
#include <piranha/detail/prepare_for_print.hpp>
#include <piranha/detail/safe_integral_adder.hpp>
#include <piranha/exceptions.hpp>
#include <piranha/is_cf.hpp>
#include <piranha/is_key.hpp>
#include <piranha/kronecker_array.hpp>
#include <piranha/math.hpp>
#include <piranha/mp_integer.hpp>
#include <piranha/mp_rational.hpp>
#include <piranha/pow.hpp>
#include <piranha/s11n.hpp>
#include <piranha/safe_cast.hpp>
#include <piranha/static_vector.hpp>
#include <piranha/symbol_utils.hpp>
#include <piranha/term.hpp>
#include <piranha/type_traits.hpp>

namespace piranha
{

// TODO is this needed?

// Fwd declaration.
template <typename>
class kronecker_monomial;
}

// Implementation of the Boost s11n api.
namespace boost
{
namespace serialization
{

template <typename Archive, typename T>
inline void save(Archive &ar, const piranha::boost_s11n_key_wrapper<piranha::kronecker_monomial<T>> &k, unsigned)
{
    if (std::is_same<Archive, boost::archive::binary_oarchive>::value) {
        piranha::boost_save(ar, k.key().get_int());
    } else {
        auto tmp = k.key().unpack(k.ss());
        piranha::boost_save(ar, tmp);
    }
}

template <typename Archive, typename T>
inline void load(Archive &ar, piranha::boost_s11n_key_wrapper<piranha::kronecker_monomial<T>> &k, unsigned)
{
    if (std::is_same<Archive, boost::archive::binary_iarchive>::value) {
        T value;
        piranha::boost_load(ar, value);
        k.key().set_int(value);
    } else {
        typename piranha::kronecker_monomial<T>::v_type tmp;
        piranha::boost_load(ar, tmp);
        if (unlikely(tmp.size() != k.ss().size())) {
            piranha_throw(std::invalid_argument, "invalid size detected in the deserialization of a Kronercker "
                                                 "monomial: the deserialized size is "
                                                     + std::to_string(tmp.size())
                                                     + " but the reference symbol set has a size of "
                                                     + std::to_string(k.ss().size()));
        }
        k.key() = piranha::kronecker_monomial<T>(tmp);
    }
}

template <typename Archive, typename T>
inline void serialize(Archive &ar, piranha::boost_s11n_key_wrapper<piranha::kronecker_monomial<T>> &k, unsigned version)
{
    split_free(ar, k, version);
}
}
}

namespace piranha
{

/// Kronecker monomial class.
/**
 * This class represents a multivariate monomial with integral exponents. The values of the exponents are packed in a
 * signed integer using Kronecker substitution, using the facilities provided by piranha::kronecker_array.
 *
 * This class satisfies the piranha::is_key, piranha::key_has_degree, piranha::key_has_ldegree and
 * piranha::key_is_differentiable type traits.
 *
 * ## Type requirements ##
 *
 * \p T must be suitable for use in piranha::kronecker_array. The default type for \p T is the signed counterpart of \p
 * std::size_t.
 *
 * ## Exception safety guarantee ##
 *
 * Unless otherwise specified, this class provides the strong exception safety guarantee for all operations.
 *
 * ## Move semantics ##
 *
 * The move semantics of this class are equivalent to the move semantics of C++ signed integral types.
 */
// NOTE:
// - consider abstracting the km_commons in a class and use it both here and in rtkm.
// - it might be better to rework the machinery for the unpacking. An idea is to just use std::vectors
//   with TLS, and have the unpack function take retval as mutable ref rather than returning a vector.
template <typename T = std::make_signed<std::size_t>::type>
class kronecker_monomial
{
public:
    /// Alias for \p T.
    using value_type = T;

private:
    using ka = kronecker_array<T>;

public:
    /// Size type.
    /**
     * Used to represent the number of variables in the monomial. Equivalent to the size type of
     * piranha::kronecker_array.
     */
    using size_type = typename ka::size_type;
    /// Vector type used for temporary packing/unpacking.
    // NOTE: this essentially defines a maximum number of small ints that can be packed in m_value,
    // as we always need to pass through pack/unpack. In practice, it does not matter: in current
    // architectures the bit width limit will result in kronecker array's limits to be smaller than
    // 255 items.
    using v_type = static_vector<T, 255u>;
    /// Arity of the multiply() method.
    static const std::size_t multiply_arity = 1u;
    /// Default constructor.
    /**
     * After construction all exponents in the monomial will be zero.
     */
    kronecker_monomial() : m_value(0)
    {
    }
    /// Defaulted copy constructor.
    kronecker_monomial(const kronecker_monomial &) = default;
    /// Defaulted move constructor.
    kronecker_monomial(kronecker_monomial &&) = default;

private:
    // Enablers for the ctor from container.
    template <typename U>
    using container_ctor_enabler
        = enable_if_t<conjunction<has_input_begin_end<const U>,
                                  has_safe_cast<T, typename std::iterator_traits<decltype(
                                                       std::begin(std::declval<const U &>()))>::value_type>>::value,
                      int>;
    // Implementation of the ctor from range.
    template <typename Iterator>
    typename v_type::size_type construct_from_range(Iterator begin, Iterator end)
    {
        v_type tmp;
        std::transform(begin, end, std::back_inserter(tmp),
                       [](const uncvref_t<decltype(*begin)> &v) { return safe_cast<T>(v); });
        m_value = ka::encode(tmp);
        return tmp.size();
    }

public:
    /// Constructor from container.
    /**
     * \note
     * This constructor is enabled only if \p U satisfies piranha::has_input_begin_end, and the value type
     * of the iterator type of \p U can be safely cast to \p T.
     *
     * This constructor will build internally a vector of values from the input container \p c, encode it and assign the
     * result to the internal integer instance. The value type of the container is converted to \p T using
     * piranha::safe_cast().
     *
     * @param c the input container.
     *
     * @throws unspecified any exception thrown by kronecker_monomial::kronecker_monomial(Iterator, Iterator).
     */
    template <typename U, container_ctor_enabler<U> = 0>
    explicit kronecker_monomial(const U &c) : kronecker_monomial(std::begin(c), std::end(c))
    {
    }

private:
    template <typename U>
    using init_list_ctor_enabler = container_ctor_enabler<std::initializer_list<U>>;

public:
    /// Constructor from initializer list.
    /**
     * \note
     * This constructor is enabled only if \p U can be safely cast to \p T.
     *
     * @param list the input initializer list.
     *
     * @throws unspecified any exception thrown by kronecker_monomial::kronecker_monomial(Iterator, Iterator).
     */
    template <typename U, init_list_ctor_enabler<U> = 0>
    explicit kronecker_monomial(std::initializer_list<U> list) : kronecker_monomial(list.begin(), list.end())
    {
    }

private:
    template <typename Iterator>
    using it_ctor_enabler
        = enable_if_t<conjunction<is_input_iterator<Iterator>,
                                  has_safe_cast<T, typename std::iterator_traits<Iterator>::value_type>>::value,
                      int>;

public:
    /// Constructor from range.
    /**
     * \note
     * This constructor is enabled only if \p Iterator is an input iterator whose value type
     * is safely convertible to \p T.
     *
     * This constructor will build internally a vector of values from the input iterators, encode it and assign the
     * result to the internal integer instance. The value type of the iterator is converted to \p T using
     * piranha::safe_cast().
     *
     * @param begin beginning of the range.
     * @param end end of the range.
     *
     * @throws unspecified any exception thrown by:
     * - piranha::kronecker_array::encode(),
     * - piranha::safe_cast(),
     * - piranha::static_vector::push_back(),
     * - increment and dereference of the input iterators.
     */
    template <typename Iterator, it_ctor_enabler<Iterator> = 0>
    explicit kronecker_monomial(Iterator begin, Iterator end)
    {
        construct_from_range(begin, end);
    }
    /// Constructor from range and symbol set.
    /**
     * \note
     * This constructor is enabled only if \p Iterator is an input iterator whose value type
     * is safely convertible to \p T.
     *
     * This constructor is identical to the constructor from range. In addition, after construction
     * it will also check that the distance between \p begin and \p end is equal to the size of \p s.
     *
     * @param begin beginning of the range.
     * @param end end of the range.
     * @param s reference piranha::symbol_fset.
     *
     * @throws std::invalid_argument if the distance between \p begin and \p end is different from
     * the size of \p s.
     * @throws unspecified any exception thrown by kronecker_monomial::kronecker_monomial(Iterator, Iterator)
     */
    template <typename Iterator, it_ctor_enabler<Iterator> = 0>
    explicit kronecker_monomial(Iterator begin, Iterator end, const symbol_fset &s)
    {
        const auto c_size = construct_from_range(begin, end);
        if (unlikely(c_size != s.size())) {
            piranha_throw(std::invalid_argument, "the Kronecker monomial constructor from range and symbol set "
                                                 "yielded an invalid monomial: the final size is "
                                                     + std::to_string(c_size) + ", while the size of the symbol set is "
                                                     + std::to_string(s.size()));
        }
    }
    /// Constructor from set of symbols.
    /**
     * After construction all exponents in the monomial will be zero.
     */
    explicit kronecker_monomial(const symbol_fset &) : kronecker_monomial()
    {
    }
    /// Converting constructor.
    /**
     * This constructor is for use when converting from one term type to another in piranha::series. It will
     * set the internal integer instance to the same value of \p other.
     *
     * @param other construction argument.
     */
    explicit kronecker_monomial(const kronecker_monomial &other, const symbol_fset &) : m_value(other.m_value)
    {
    }
    /// Constructor from \p T.
    /**
     * This constructor will initialise the internal integer instance
     * to \p n.
     *
     * @param n initializer for the internal integer instance.
     */
    explicit kronecker_monomial(const T &n) : m_value(n)
    {
    }
    /// Destructor.
    ~kronecker_monomial()
    {
        PIRANHA_TT_CHECK(is_key, kronecker_monomial);
        PIRANHA_TT_CHECK(key_has_degree, kronecker_monomial);
        PIRANHA_TT_CHECK(key_has_ldegree, kronecker_monomial);
        PIRANHA_TT_CHECK(key_is_differentiable, kronecker_monomial);
    }
    /// Copy assignment operator.
    /**
     * @param other the assignment argument.
     *
     * @return a reference to \p this.
     *
     * @throws unspecified any exception thrown by the assignment operator of the base class.
     */
    kronecker_monomial &operator=(const kronecker_monomial &other) = default;
    /// Defaulted move assignment operator.
    /**
     * @param other the assignment argument.
     *
     * @return a reference to \p this.
     */
    kronecker_monomial &operator=(kronecker_monomial &&other) = default;
    /// Set the internal integer instance.
    /**
     * @param n value to which the internal integer instance will be set.
     */
    void set_int(const T &n)
    {
        m_value = n;
    }
    /// Get internal instance.
    /**
     * @return value of the internal integer instance.
     */
    T get_int() const
    {
        return m_value;
    }
    /// Compatibility check.
    /**
     * A monomial is considered incompatible if any of these conditions holds:
     *
     * - the size of \p args is zero and the internal integer is not zero,
     * - the size of \p args is equal to or larger than the size of the output of
     *   piranha::kronecker_array::get_limits(),
     * - the internal integer is not within the limits reported by piranha::kronecker_array::get_limits().
     *
     * Otherwise, the monomial is considered to be compatible for insertion.
     *
     * @param args reference piranha::symbol_fset.
     *
     * @return compatibility flag for the monomial.
     */
    bool is_compatible(const symbol_fset &args) const noexcept
    {
        // NOTE: the idea here is to avoid unpack()ing for performance reasons: these checks
        // are already part of unpack(), and that's why unpack() is used instead of is_compatible()
        // in other methods.
        const auto s = args.size();
        // No args means the value must also be zero.
        if (!s) {
            return !m_value;
        }
        const auto &limits = ka::get_limits();
        // If we overflow the maximum size available, we cannot use this object as key in series.
        if (s >= limits.size()) {
            return false;
        }
        const auto &l = limits[static_cast<decltype(limits.size())>(s)];
        // Value is compatible if it is within the bounds for the given size.
        return (m_value >= std::get<1u>(l) && m_value <= std::get<2u>(l));
    }
    /// Zero check.
    /**
     * A monomial is never zero.
     *
     * @return \p false.
     */
    bool is_zero(const symbol_fset &) const noexcept
    {
        return false;
    }
    /// Merge symbols.
    /**
     * This method will return a copy of \p this in which the value 0 has been inserted
     * at the positions specified by \p ins_map. Specifically, a number of zeroes equal to the size of
     * the corresponding piranha::symbol_fset will be inserted before each index appearing in \p ins_map.
     *
     * For instance, given a piranha::kronecker_monomial containing the values <tt>[1,2,3,4]</tt>, a symbol set
     * \p args containing <tt>["c","e","g","h"]</tt> and an insertion map \p ins_map containing the pairs
     * <tt>[(0,["a","b"]),(1,["d"]),(2,["f"]),(4,["i"])]</tt>, the output of this method will be
     * <tt>[0,0,1,0,2,0,3,4,0]</tt>. That is, the symbols appearing in \p ins_map are merged into \p this
     * with a value of zero at the specified positions.
     *
     * @param args reference symbol set for \p this.
     * @param ins_map the insertion map.
     *
     * @return a piranha::kronecker_monomial resulting from inserting into \p this zeroes at the positions
     * specified by \p ins_map.
     *
     * @throws std::invalid_argument in the following cases:
     * - the size of \p ins_map is zero,
     * - the last index in \p ins_map is greater than the size of \p args.
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - piranha::static_vector::push_back(),
     * - piranha::kronecker_array::encode().
     */
    kronecker_monomial merge_symbols(const symbol_idx_fmap<symbol_fset> &ins_map, const symbol_fset &args) const
    {
        return kronecker_monomial(detail::km_merge_symbols<v_type, ka>(ins_map, args, m_value));
    }
    /// Check if monomial is unitary.
    /**
     * @return \p true if all the exponents are zero, \p false otherwise.
     */
    bool is_unitary(const symbol_fset &) const
    {
        // A kronecker code will be zero if all components are zero.
        return !m_value;
    }

private:
    // Degree utils.
    using degree_type = decltype(std::declval<const T &>() + std::declval<const T &>());

public:
    /// Degree.
    /**
     * The type returned by this method is the type resulting from the addition of two instances
     * of \p T.
     *
     * @param args reference piranha::symbol_fset.
     *
     * @return degree of the monomial.
     *
     * @throws std::overflow_error if the computation of the degree overflows.
     * @throws unspecified any exception thrown by unpack().
     */
    degree_type degree(const symbol_fset &args) const
    {
        const auto tmp = unpack(args);
        // NOTE: this should be guaranteed by the unpack function.
        piranha_assert(tmp.size() == args.size());
        degree_type retval(0);
        for (const auto &x : tmp) {
            // NOTE: here it might be possible to demonstrate that overflow can
            // never occur, and that we can use a normal integral addition.
            detail::safe_integral_adder(retval, static_cast<degree_type>(x));
        }
        return retval;
    }
    /// Low degree (equivalent to the degree).
    /**
     * @param args reference piranha::symbol_fset.
     *
     * @return the output of degree(const symbol_fset &) const.
     *
     * @throws unspecified any exception thrown by degree(const symbol_fset &) const.
     */
    degree_type ldegree(const symbol_fset &args) const
    {
        return degree(args);
    }
    /// Partial degree.
    /**
     * Partial degree of the monomial: only the symbols at the positions specified by \p p are considered.
     * The type returned by this method is the type resulting from the addition of two instances
     * of \p T.
     *
     * @param p positions of the symbols to be considered in the calculation of the degree.
     * @param args reference piranha::symbol_fset.
     *
     * @return the summation of the exponents of the monomial at the positions specified by \p p.
     *
     * @throws std::overflow_error if the computation of the degree overflows.
     * @throws unspecified any exception thrown by unpack().
     */
    degree_type degree(const symbol_idx_fset &p, const symbol_fset &args) const
    {
        const auto tmp = unpack(args);
        piranha_assert(tmp.size() == args.size());
        const auto cit = tmp.begin();
        degree_type retval(0);
        for (auto it = p.begin(); it != p.end() && *it < tmp.size(); ++it) {
            detail::safe_integral_adder(retval, static_cast<degree_type>(cit[*it]));
        }
        return retval;
    }
    /// Partial low degree (equivalent to the partial degree).
    /**
     * @param p positions of the symbols to be considered in the calculation of the degree.
     * @param args reference piranha::symbol_fset.
     *
     * @return the output of degree(const symbol_idx_fset &, const symbol_fset &) const.
     *
     * @throws unspecified any exception thrown by degree(const symbol_idx_fset &, const symbol_fset &) const.
     */
    degree_type ldegree(const symbol_idx_fset &p, const symbol_fset &args) const
    {
        return degree(p, args);
    }

private:
    // Enabler for multiply().
    template <typename Cf>
    using multiply_enabler = enable_if_t<std::is_same<detail::cf_mult_enabler<Cf>, void>::value, int>;

public:
    /// Multiply terms with a Kronecker monomial key.
    /**
     * \note
     * This method is enabled only if \p Cf satisfies piranha::is_cf and piranha::has_mul3.
     *
     * Multiply \p t1 by \p t2, storing the result in the only element of \p res. This method
     * offers the basic exception safety guarantee. If \p Cf is an instance of piranha::mp_rational, then
     * only the numerators of the coefficients will be multiplied.
     *
     * Note that the key of the return value is generated directly from the addition of the values of the input keys.
     * No check is performed for overflow of either the limits of the integral type or the limits of the Kronecker
     * codification.
     *
     * @param res return value.
     * @param t1 first argument.
     * @param t2 second argument.
     *
     * @throws unspecified any exception thrown by piranha::math::mul3().
     */
    template <typename Cf, multiply_enabler<Cf> = 0>
    static void multiply(std::array<term<Cf, kronecker_monomial>, multiply_arity> &res,
                         const term<Cf, kronecker_monomial> &t1, const term<Cf, kronecker_monomial> &t2,
                         const symbol_fset &)
    {
        // Coefficient first.
        detail::cf_mult_impl(res[0u].m_cf, t1.m_cf, t2.m_cf);
        // Now the key.
        math::add3(res[0u].m_key.m_value, t1.m_key.get_int(), t2.m_key.get_int());
    }
    /// Multiply Kronecker monomials.
    /**
     * Multiply \p a by \p b, storing the result in \p res.
     * No check is performed for overflow of either the limits of the integral type or the limits of the Kronecker
     * codification.
     *
     * @param res return value.
     * @param a first argument.
     * @param b second argument.
     */
    static void multiply(kronecker_monomial &res, const kronecker_monomial &a, const kronecker_monomial &b,
                         const symbol_fset &)
    {
        math::add3(res.m_value, a.m_value, b.m_value);
    }
    /// Hash value.
    /**
     * @return the internal integer instance, cast to \p std::size_t.
     */
    std::size_t hash() const
    {
        return static_cast<std::size_t>(m_value);
    }
    /// Equality operator.
    /**
     * @param other comparison argument.
     *
     * @return \p true if the internal integral instance of \p this is equal to the integral instance of \p other,
     * \p false otherwise.
     */
    bool operator==(const kronecker_monomial &other) const
    {
        return m_value == other.m_value;
    }
    /// Inequality operator.
    /**
     * @param other comparison argument.
     *
     * @return the opposite of operator==().
     */
    bool operator!=(const kronecker_monomial &other) const
    {
        return m_value != other.m_value;
    }
    /// Name of the linear argument.
    /**
     * If the monomial is linear in a variable (i.e., all exponents are zero apart from a single unitary
     * exponent), the name of the variable will be returned. Otherwise, an error will be raised.
     *
     * @param args reference piranha::symbol_fset.
     *
     * @return name of the linear variable.
     *
     * @throws std::invalid_argument if the monomial is not linear.
     * @throws unspecified any exception thrown by unpack().
     */
    std::string linear_argument(const symbol_fset &args) const
    {
        const auto v = unpack(args);
        const auto size = v.size();
        typename v_type::size_type n_linear = 0, candidate = 0;
        auto it_args = args.begin(), it_cand = it_args;
        for (typename v_type::size_type i = 0; i < size; ++i, ++it_args) {
            if (!v[i]) {
                continue;
            }
            if (unlikely(v[i] != T(1))) {
                piranha_throw(std::invalid_argument, "while attempting to extract the linear argument "
                                                     "from a Kronecker monomial, a non-unitary exponent was "
                                                     "encountered in correspondence of the variable '"
                                                         + (*it_args) + "'");
            }
            candidate = i;
            it_cand = it_args;
            ++n_linear;
        }
        if (unlikely(n_linear != 1u)) {
            piranha_throw(std::invalid_argument, "the extraction of the linear argument "
                                                 "from a Kronecker monomial failed: the monomial is not linear");
        }
        return *it_cand;
    }

private:
    // Enabler for pow.
    template <typename U>
    using pow_enabler
        = enable_if_t<has_safe_cast<T, decltype(std::declval<const integer &>() * std::declval<const U &>())>::value,
                      int>;

public:
    /// Exponentiation.
    /**
     * \note
     * This method is enabled only if \p U is multipliable by piranha::integer and the result type can be
     * safely cast back to \p T.
     *
     * This method will return a monomial corresponding to \p this raised to the <tt>x</tt>-th power. The exponentiation
     * is computed via the multiplication of the exponents promoted to piranha::integer by \p x. The result will
     * be cast back to \p T via piranha::safe_cast().
     *
     * @param x exponent.
     * @param args reference piranha::symbol_fset.
     *
     * @return \p this to the power of \p x.
     *
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - piranha::safe_cast(),
     * - the constructor and multiplication operator of piranha::integer,
     * - piranha::kronecker_array::encode().
     */
    template <typename U, pow_enabler<U> = 0>
    kronecker_monomial pow(const U &x, const symbol_fset &args) const
    {
        auto v = unpack(args);
        for (auto &n : v) {
            n = safe_cast<T>(integer(n) * x);
        }
        kronecker_monomial retval;
        retval.m_value = ka::encode(v);
        return retval;
    }
    /// Unpack internal integer instance.
    /**
     * This method will decode the internal integral instance into a piranha::static_vector of size equal to the size of
     * \p args.
     *
     * @param args reference piranha::symbol_fset.
     *
     * @return piranha::static_vector containing the result of decoding the internal integral instance via
     * piranha::kronecker_array.
     *
     * @throws std::invalid_argument if the size of \p args is larger than the maximum size of piranha::static_vector.
     * @throws unspecified any exception thrown by piranha::kronecker_array::decode().
     */
    v_type unpack(const symbol_fset &args) const
    {
        return detail::km_unpack<v_type, ka>(args, m_value);
    }
    /// Print.
    /**
     * This method will print to stream a human-readable representation of the monomial.
     *
     * @param os target stream.
     * @param args reference piranha::symbol_fset.
     *
     * @throws unspecified any exception thrown by unpack() or by streaming instances of \p T.
     */
    void print(std::ostream &os, const symbol_fset &args) const
    {
        const auto tmp = unpack(args);
        piranha_assert(tmp.size() == args.size());
        const T zero(0), one(1);
        bool empty_output = true;
        auto it_args = args.begin();
        for (decltype(tmp.size()) i = 0u; i < tmp.size(); ++i, ++it_args) {
            if (tmp[i] != zero) {
                if (!empty_output) {
                    os << '*';
                }
                os << *it_args;
                empty_output = false;
                if (tmp[i] != one) {
                    os << "**" << detail::prepare_for_print(tmp[i]);
                }
            }
        }
    }
    /// Print in TeX mode.
    /**
     * This method will print to stream a TeX representation of the monomial.
     *
     * @param os target stream.
     * @param args reference piranha::symbol_fset.
     *
     * @throws unspecified any exception thrown by unpack() or by streaming instances of \p T.
     */
    void print_tex(std::ostream &os, const symbol_fset &args) const
    {
        const auto tmp = unpack(args);
        std::ostringstream oss_num, oss_den, *cur_oss;
        const T zero(0), one(1);
        T cur_value;
        auto it_args = args.begin();
        for (decltype(tmp.size()) i = 0u; i < tmp.size(); ++i, ++it_args) {
            cur_value = tmp[i];
            if (cur_value != zero) {
                // NOTE: here negate() is safe because of the symmetry in kronecker_array.
                cur_oss
                    = (cur_value > zero) ? std::addressof(oss_num) : (math::negate(cur_value), std::addressof(oss_den));
                (*cur_oss) << "{" << *it_args << "}";
                if (cur_value != one) {
                    (*cur_oss) << "^{" << static_cast<long long>(cur_value) << "}";
                }
            }
        }
        const std::string num_str = oss_num.str(), den_str = oss_den.str();
        if (!num_str.empty() && !den_str.empty()) {
            os << "\\frac{" << num_str << "}{" << den_str << "}";
        } else if (!num_str.empty() && den_str.empty()) {
            os << num_str;
        } else if (num_str.empty() && !den_str.empty()) {
            os << "\\frac{1}{" << den_str << "}";
        }
    }
    /// Partial derivative.
    /**
     * This method will return the partial derivative of \p this with respect to the symbol at the position indicated by
     * \p p. The result is a pair consisting of the exponent associated to \p p before differentiation and the monomial
     * itself after differentiation. If \p p is not smaller than the size of \p args or if its corresponding exponent is
     * zero, the returned pair will be <tt>(0,kronecker_monomial{args})</tt>.
     *
     * @param p position of the symbol with respect to which the differentiation will be calculated.
     * @param args reference piranha::symbol_fset.
     *
     * @return result of the differentiation.
     *
     * @throws std::overflow_error if the computation of the derivative causes a negative overflow.
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - piranha::kronecker_array::encode().
     */
    std::pair<T, kronecker_monomial> partial(const symbol_idx &p, const symbol_fset &args) const
    {
        auto v = unpack(args);
        if (p >= args.size() || v[static_cast<size_type>(p)] == T(0)) {
            // Derivative wrt a variable not in the monomial: position is outside the bounds, or it refers to a
            // variable with zero exponent.
            return {T(0), kronecker_monomial(args)};
        }
        auto v_b = v.begin();
        // Original exponent.
        T n(v_b[p]);
        // Decrement the exponent in the monomial.
        if (unlikely(n == std::numeric_limits<T>::min())) {
            piranha_throw(std::overflow_error, "negative overflow error in the calculation of the "
                                               "partial derivative of a Kronecker monomial");
        }
        v_b[p] = static_cast<T>(n - T(1));
        kronecker_monomial tmp_km;
        tmp_km.m_value = ka::encode(v);
        return {n, tmp_km};
    }
    /// Integration.
    /**
     * This method will return the antiderivative of \p this with respect to the symbol \p s. The result is a pair
     * consisting of the exponent associated to \p s increased by one and the monomial itself
     * after integration. If \p s is not in \p args, the returned monomial will have an extra exponent
     * set to 1 in the same position \p s would have if it were added to \p args.
     * If the exponent corresponding to \p s is -1, an error will be produced.
     *
     * @param s symbol with respect to which the integration will be calculated.
     * @param args reference piranha::symbol_fset.
     *
     * @return result of the integration.
     *
     * @throws std::invalid_argument if the exponent associated to \p s is -1 or if the value of an exponent overflows.
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - piranha::static_vector::push_back(),
     * - piranha::kronecker_array::encode().
     */
    std::pair<T, kronecker_monomial> integrate(const std::string &s, const symbol_fset &args) const
    {
        v_type v = unpack(args), retval;
        T expo(0), one(1);
        auto it_args = args.begin();
        for (size_type i = 0; i < v.size(); ++i, ++it_args) {
            if (expo == T(0) && s < *it_args) {
                // If we went past the position of s in args and still we
                // have not performed the integration, it means that we need to add
                // a new exponent.
                retval.push_back(one);
                expo = one;
            }
            retval.push_back(v[i]);
            if (*it_args == s) {
                // NOTE: here using i is safe: if retval gained an extra exponent in the condition above,
                // we are never going to land here as *it_args is at this point never going to be s.
                if (unlikely(retval[i] == std::numeric_limits<T>::max())) {
                    piranha_throw(std::overflow_error,
                                  "positive overflow error in the calculation of the integral of a Kronecker monomial");
                }
                retval[i] = static_cast<T>(retval[i] + T(1));
                if (math::is_zero(retval[i])) {
                    piranha_throw(std::invalid_argument,
                                  "unable to perform Kronecker monomial integration: a negative "
                                  "unitary exponent was encountered in correspondence of the variable '"
                                      + (*it_args) + "'");
                }
                expo = retval[i];
            }
        }
        // If expo is still zero, it means we need to add a new exponent at the end.
        if (expo == T(0)) {
            retval.push_back(one);
            expo = one;
        }
        return {expo, kronecker_monomial{ka::encode(retval)}};
    }

private:
    // Determination of the eval type.
    template <typename U>
    using e_type = decltype(math::pow(std::declval<const U &>(), std::declval<const T &>()));
    template <typename U>
    using eval_type
        = enable_if_t<conjunction<is_multipliable_in_place<e_type<U>>, std::is_constructible<e_type<U>, int>>::value,
                      e_type<U>>;

public:
    /// Evaluation.
    /**
     * \note
     * This method is available only if \p U satisfies the following requirements:
     * - it can be used in piranha::math::pow() with the monomial exponents as powers, yielding a type \p eval_type,
     * - \p eval_type is constructible from \p int,
     * - \p eval_type is multipliable in place.
     *
     * The return value will be built by iteratively applying piranha::math::pow() using the values provided
     * by \p values as bases and the values in the monomial as exponents. If the size of the monomial is zero, 1 will be
     * returned.
     *
     * @param values the values will be used for substitution.
     * @param args reference piranha::symbol_fset.
     *
     * @return the result of evaluating \p this with the values provided in \p values.
     *
     * @throws std::invalid_argument if the size of \p values and \p args differ.
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - construction of the return type,
     * - piranha::math::pow() or the in-place multiplication operator of the return type.
     */
    template <typename U>
    eval_type<U> evaluate(const std::vector<U> &values, const symbol_fset &args) const
    {
        using return_type = eval_type<U>;
        using size_type = typename v_type::size_type;
        // NOTE: here we can check the values size only against args.
        if (unlikely(values.size() != args.size())) {
            piranha_throw(std::invalid_argument,
                          "invalid vector of values for Kronecker monomial evaluation: the vector of values has size "
                              + std::to_string(values.size()) + ", while the reference set of symbols has a size of "
                              + std::to_string(args.size()));
        }
        auto v = unpack(args);
        return_type retval(1);
        for (size_type i = 0; i < v.size(); ++i) {
            retval *= math::pow(values[static_cast<typename std::vector<U>::size_type>(i)], v[i]);
        }
        return retval;
    }

private:
    // Subs utilities. Subs type is same as e_type.
    template <typename U>
    using subs_type = enable_if_t<std::is_constructible<e_type<U>, int>::value, e_type<U>>;

public:
    /// Substitution.
    /**
     * \note
     * This method is enabled only if:
     * - \p U can be raised to the value type, yielding a type \p subs_type,
     * - \p subs_type can be constructed from \p int.
     *
     * This method will substitute the symbol at the position \p p in the monomial with the quantity \p x. The return
     * value is a vector containing one pair in which the first element is the result of the substitution (i.e., \p x
     * raised to the power of the exponent corresponding to \p p), and the second element is the monomial after the
     * substitution has been performed (i.e., with the exponent corresponding to \p p set to zero). If \p p is not less
     * than the size of \p args, the return value will be <tt>(1,this)</tt> (i.e., the monomial is unchanged and the
     * substitution yields 1).
     *
     * @param p position of the symbol that will be substituted.
     * @param x quantity that will be substituted.
     * @param args reference piranha::symbol_fset.
     *
     * @return the result of substituting \p x for the symbol at the position \p p.
     *
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - the construction of the return value,
     * - piranha::math::pow(),
     * - piranha::static_vector::push_back(),
     * - piranha::kronecker_array::encode().
     */
    template <typename U>
    std::vector<std::pair<subs_type<U>, kronecker_monomial>> subs(const symbol_idx &p, const U &x,
                                                                  const symbol_fset &args) const
    {
        if (p < args.size()) {
            // If the position is within the monomial, the result of the subs will come from pow() and
            // we will have to set to 0 the affected symbol.
            auto v = unpack(args);
            v[static_cast<size_type>(p)] = T(0);
            return {{math::pow(x, v[static_cast<size_type>(p)]), kronecker_monomial{ka::encode(v)}}};
        }
        // Otherwise, the substitution yields 1 and the monomial is the original one.
        return {{subs_type<U>(1), *this}};
    }

private:
    // ipow subs utilities.
    template <typename U>
    using ips_type = decltype(math::pow(std::declval<const U &>(), std::declval<const integer &>()));
    template <typename U>
    using ipow_subs_type = enable_if_t<std::is_constructible<ips_type<U>, int>::value, ips_type<U>>;

public:
    /// Substitution of integral power.
    /**
     * \note
     * This method is enabled only if:
     * - \p U can be raised to a piranha::integer power, yielding a type \p subs_type,
     * - \p subs_type is constructible from \p int.
     *
     * This method will substitute the <tt>n</tt>-th power of the symbol at the position \p p with the quantity \p x.
     * The return value is a vector containing a single pair in which the first element is the result of the
     * substitution, and the second element the monomial after the substitution has been performed.
     * If \p p is not less than the size of \p args, the return value will be <tt>(1,this)</tt> (i.e., the monomial is
     * unchanged and the substitution yields 1).
     *
     * The method will substitute also powers higher than \p n in absolute value.
     * For instance, the substitution of <tt>y**2</tt> with \p a in the monomial <tt>y**7</tt> will produce
     * <tt>a**3 * y</tt>, and the substitution of <tt>y**-2</tt> with \p a in the monomial <tt>y**-7</tt> will produce
     * <tt>a**3 * y**-1</tt>.
     *
     * @param p position of the symbol that will be substituted.
     * @param n integral power that will be substituted.
     * @param x quantity that will be substituted.
     * @param args reference piranha::symbol_fset.
     *
     * @return the result of substituting \p x for the <tt>n</tt>-th power of the symbol at the position \p p.
     *
     * @throws std::invalid_argument is \p n is zero.
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - construction of the return value,
     * - piranha::math::pow(),
     * - arithmetics on piranha::integer,
     * - the in-place subtraction operator of the exponent type,
     * - piranha::kronecker_array::encode().
     */
    template <typename U>
    std::vector<std::pair<ipow_subs_type<U>, kronecker_monomial>> ipow_subs(const symbol_idx &p, const integer &n,
                                                                            const U &x, const symbol_fset &args) const
    {
        if (unlikely(!n.sgn())) {
            piranha_throw(std::invalid_argument,
                          "invalid integral exponent in ipow_subs(): the exponent must be nonzero");
        }
        if (p < args.size()) {
            auto v = unpack(args);
            const auto q = v[static_cast<size_type>(p)] / n;
            if (q >= 1) {
                v[static_cast<size_type>(p)] -= q * n;
                return {{math::pow(x, q), kronecker_monomial{ka::encode(v)}}};
            }
        }
        // Otherwise, the substitution yields 1 and the monomial is the original one.
        return {{ipow_subs_type<U>(1), *this}};
    }
    /// Identify symbols that can be trimmed.
    /**
     * This method is used in piranha::series::trim(). The input parameter \p candidates
     * contains a map of symbol indices in \p args that are candidates for elimination. The method will set
     * to \p false the mapped values in \p candidates whose indices correspond to nonzero elements in \p this.
     *
     * @param candidates map of candidate indices for elimination.
     * @param args reference piranha::symbol_fset.
     *
     * @throws std::invalid_argument in the following cases:
     * - the size of \p candidates differs from the size of \p args,
     * - the index of the last element of \p candidates, if it exists, is not equal to the size of \p args minus one.
     * @throws unspecified any exception thrown by unpack().
     */
    void trim_identify(symbol_idx_fmap<bool> &candidates, const symbol_fset &args) const
    {
        return detail::km_trim_identify<v_type, ka>(candidates, args, m_value);
    }
    /// Trim.
    /**
     * This method will return a copy of \p this without the elements at the indices specified by \p trim_idx.
     *
     * @param trim_idx indices of the elements which will be removed.
     * @param args reference piranha::symbol_fset.
     *
     * @return a trimmed copy of \p this.
     *
     * @throws unspecified any exception thrown by:
     * - unpack(),
     * - piranha::static_vector::push_back().
     */
    kronecker_monomial trim(const symbol_idx_fset &trim_idx, const symbol_fset &args) const
    {
        return kronecker_monomial{detail::km_trim<v_type, ka>(trim_idx, args, m_value)};
    }
    /// Comparison operator.
    /**
     * @param other comparison argument.
     *
     * @return \p true if the internal integral value of \p this is less than the internal
     * integral value of \p other, \p false otherwise.
     */
    bool operator<(const kronecker_monomial &other) const
    {
        return m_value < other.m_value;
    }

#if defined(PIRANHA_WITH_MSGPACK)
private:
    // Enablers for msgpack serialization.
    template <typename Stream>
    using msgpack_pack_enabler = enable_if_t<conjunction<is_msgpack_stream<Stream>, has_msgpack_pack<Stream, T>,
                                                         has_msgpack_pack<Stream, v_type>>::value,
                                             int>;
    template <typename U>
    using msgpack_convert_enabler = enable_if_t<conjunction<has_msgpack_convert<typename U::value_type>,
                                                            has_msgpack_convert<typename U::v_type>>::value,
                                                int>;

public:
    /// Serialize in msgpack format.
    /**
     * \note
     * This method is activated only if \p Stream satisfies piranha::is_msgpack_stream and both \p T and
     * piranha::kronecker_monomial::v_type satisfy piranha::has_msgpack_pack.
     *
     * This method will pack \p this into \p packer. The packed object is the internal integral instance in binary
     * format, an array of exponents in portable format.
     *
     * @param packer the target packer.
     * @param f the serialization format.
     * @param s reference piranha::symbol_fset.
     *
     * @throws unspecified any exception thrown by unpack() or piranha::msgpack_pack().
     */
    template <typename Stream, msgpack_pack_enabler<Stream> = 0>
    void msgpack_pack(msgpack::packer<Stream> &packer, msgpack_format f, const symbol_fset &s) const
    {
        if (f == msgpack_format::binary) {
            piranha::msgpack_pack(packer, m_value, f);
        } else {
            auto tmp = unpack(s);
            piranha::msgpack_pack(packer, tmp, f);
        }
    }
    /// Deserialize from msgpack object.
    /**
     * \note
     * This method is activated only if both \p T and piranha::kronecker_monomial::v_type satisfy
     * piranha::has_msgpack_convert.
     *
     * This method will deserialize \p o into \p this. In binary mode, no check is performed on the content of \p o,
     * and calling this method will result in undefined behaviour if \p o does not contain a monomial serialized via
     * msgpack_pack().
     *
     * @param o msgpack object that will be deserialized.
     * @param f serialization format.
     * @param s reference piranha::symbol_fset.
     *
     * @throws std::invalid_argument if the size of the deserialized array differs from the size of \p s.
     * @throws unspecified any exception thrown by:
     * - the constructor of piranha::kronecker_monomial from a container,
     * - piranha::msgpack_convert().
     */
    template <typename U = kronecker_monomial, msgpack_convert_enabler<U> = 0>
    void msgpack_convert(const msgpack::object &o, msgpack_format f, const symbol_fset &s)
    {
        if (f == msgpack_format::binary) {
            piranha::msgpack_convert(m_value, o, f);
        } else {
            v_type tmp;
            piranha::msgpack_convert(tmp, o, f);
            if (unlikely(tmp.size() != s.size())) {
                piranha_throw(std::invalid_argument, "incompatible symbol set in monomial serialization: the reference "
                                                     "symbol set has a size of "
                                                         + std::to_string(s.size())
                                                         + ", while the monomial being deserialized has a size of "
                                                         + std::to_string(tmp.size()));
            }
            *this = kronecker_monomial(tmp);
        }
    }
#endif

private:
    T m_value;
};

/// Alias for piranha::kronecker_monomial with default type.
using k_monomial = kronecker_monomial<>;

inline namespace impl
{

template <typename Archive, typename T>
using k_monomial_boost_save_enabler
    = enable_if_t<conjunction<has_boost_save<Archive, T>,
                              has_boost_save<Archive, typename kronecker_monomial<T>::v_type>>::value>;

template <typename Archive, typename T>
using k_monomial_boost_load_enabler
    = enable_if_t<conjunction<has_boost_load<Archive, T>,
                              has_boost_load<Archive, typename kronecker_monomial<T>::v_type>>::value>;
}

/// Specialisation of piranha::boost_save() for piranha::kronecker_monomial.
/**
 * \note
 * This specialisation is enabled only if \p T and piranha::kronecker_monomial::v_type satisfy
 * piranha::has_boost_save.
 *
 * If \p Archive is \p boost::archive::binary_oarchive, the internal integral instance is saved.
 * Otherwise, the monomial is unpacked and the vector of exponents is saved.
 *
 * @throws unspecified any exception thrown by piranha::boost_save() or piranha::kronecker_monomial::unpack().
 */
template <typename Archive, typename T>
struct boost_save_impl<Archive, boost_s11n_key_wrapper<kronecker_monomial<T>>,
                       k_monomial_boost_save_enabler<Archive, T>>
    : boost_save_via_boost_api<Archive, boost_s11n_key_wrapper<kronecker_monomial<T>>> {
};

/// Specialisation of piranha::boost_load() for piranha::kronecker_monomial.
/**
 * \note
 * This specialisation is enabled only if \p T and piranha::kronecker_monomial::v_type satisfy
 * piranha::has_boost_load.
 *
 * @throws std::invalid_argument if the size of the serialized monomial is different from the size of the symbol set.
 * @throws unspecified any exception thrown by:
 * - piranha::boost_load(),
 * - the constructor of piranha::kronecker_monomial from a container.
 */
template <typename Archive, typename T>
struct boost_load_impl<Archive, boost_s11n_key_wrapper<kronecker_monomial<T>>,
                       k_monomial_boost_load_enabler<Archive, T>>
    : boost_load_via_boost_api<Archive, boost_s11n_key_wrapper<kronecker_monomial<T>>> {
};
}

namespace std
{

/// Specialisation of \p std::hash for piranha::kronecker_monomial.
template <typename T>
struct hash<piranha::kronecker_monomial<T>> {
    /// Result type.
    using result_type = size_t;
    /// Argument type.
    using argument_type = piranha::kronecker_monomial<T>;
    /// Hash operator.
    /**
     * @param a argument whose hash value will be computed.
     *
     * @return hash value of \p a computed via piranha::kronecker_monomial::hash().
     */
    result_type operator()(const argument_type &a) const
    {
        return a.hash();
    }
};
}

#endif
