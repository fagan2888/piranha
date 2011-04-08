/***************************************************************************
 *   Copyright (C) 2009-2011 by Francesco Biscani                          *
 *   bluescarni@gmail.com                                                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef PIRANHA_ECHELON_SIZE_HPP
#define PIRANHA_ECHELON_SIZE_HPP

#include <boost/concept/assert.hpp>
#include <boost/integer_traits.hpp>
#include <cstddef>
#include <type_traits>

#include "concepts/term.hpp"
#include "detail/base_series_fwd.hpp"

namespace piranha
{

namespace detail
{

template <typename CfSeries, std::size_t Level = 0, typename Enable = void>
struct echelon_level_impl
{
	static_assert(Level < boost::integer_traits<std::size_t>::const_max,"Overflow error.");
	static const std::size_t value = echelon_level_impl<typename CfSeries::term_type::cf_type,Level + static_cast<std::size_t>(1)>::value;
};

template <typename Cf, std::size_t Level>
struct echelon_level_impl<Cf,Level,typename std::enable_if<!std::is_base_of<base_series_tag,Cf>::value>::type>
{
	static const std::size_t value = Level;
};

}

/// Echelon size type-trait.
/**
 * Echelon size of \p Term. The echelon size is defined recursively by the number of times coefficient types are series, in \p Term
 * and its nested types.
 * 
 * For instance, polynomials have numerical coefficients, hence their echelon size is 1 (numerical coefficients are not series, hence they act
 * as terminators in this recursion). Fourier series are also series with numerical coefficients,
 * hence their echelon size is also 1. Poisson series are Fourier series with polynomial coefficients, hence their echelon size is 2: the polynomial
 * coefficients are series whose coefficients are not series.
 * 
 * \section type_requirements Type requirements
 * 
 * \p Term must be a model of piranha::concept::Term.
 */
template <typename Term>
struct echelon_size
{
	private:
		BOOST_CONCEPT_ASSERT((concept::Term<Term>));
		static_assert(detail::echelon_level_impl<typename Term::cf_type>::value < boost::integer_traits<std::size_t>::const_max,"Overflow error.");
	public:
		/// Value of echelon size.
		static const std::size_t value = detail::echelon_level_impl<typename Term::cf_type>::value + static_cast<std::size_t>(1);
};

// Instantiation of type-trait value.
template <typename Term>
const std::size_t echelon_size<Term>::value;

}

#endif
