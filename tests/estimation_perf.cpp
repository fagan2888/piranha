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

#include "../src/base_series_multiplier.hpp"

#define BOOST_TEST_MODULE estimation_test
#include <boost/test/unit_test.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/timer/timer.hpp>
#include <cstddef>
#include <iostream>
#include <utility>

#include "../src/environment.hpp"
#include "../src/kronecker_monomial.hpp"
#include "../src/polynomial.hpp"
#include "../src/pow.hpp"
#include "../src/power_series.hpp"
#include "../src/settings.hpp"

using namespace piranha;
using t_type = boost::timer::auto_cpu_timer;
using p_type = polynomial<double,k_monomial>;

static auto max_nt = []() {
	unsigned retval = 1u;
	if (boost::unit_test::framework::master_test_suite().argc > 1) {
		retval = boost::lexical_cast<unsigned>(boost::unit_test::framework::master_test_suite().argv[1u]);
	}
	if (retval == 0u) {
		retval = 1u;
	}
	return retval;
};

struct multiplier: base_series_multiplier<p_type>
{
	using base = base_series_multiplier<p_type>;
	using base::base;
	struct p_mult: base::plain_multiplier<false>
	{
		using m_base = base::plain_multiplier<false>;
		using m_base::m_base;
	};
	struct ff
	{
		using size_type = base::size_type;
		ff(const multiplier *m, int limit):m_m(m),m_limit(limit) {}
		unsigned operator()(const size_type &i, const size_type &j) const
		{
			return detail::ps_get_degree(*m_m->m_v1[i],m_m->m_ss) + detail::ps_get_degree(*m_m->m_v2[j],m_m->m_ss) > m_limit;
		}
		const multiplier	*m_m;
		const int		m_limit;
	};
	template <std::size_t MultArity, typename MultFunctor, typename ... Args>
	p_type::size_type estimate_final_series_size(Args && ... args) const
	{
		return base::estimate_final_series_size<MultArity,MultFunctor>(std::forward<Args>(args)...);
	}
};

BOOST_AUTO_TEST_CASE(initial_setup)
{
	environment env;
	std::cout << "Max number of threads: " << max_nt() << '\n';
	settings::set_min_work_per_thread(1u);
}

BOOST_AUTO_TEST_CASE(fateman1_test)
{
	std::cout << "Fateman 1:\n";
	std::cout << "==========\n\n";
	p_type x("x"), y("y"), z("z"), t("t");
	auto f = x + y + z + t + 1;
	auto tmp(f);
	for (auto i = 1; i < 20; ++i) {
		f *= tmp;
	}
	auto g = f + 1;
	const double real_size = 135751.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>() << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(fateman1_truncated_test)
{
	std::cout << "Fateman 1 truncated:\n";
	std::cout << "====================\n\n";
	p_type x("x"), y("y"), z("z"), t("t");
	auto f = x + y + z + t + 1;
	auto tmp(f);
	for (auto i = 1; i < 20; ++i) {
		f *= tmp;
	}
	auto g = f + 1;
	const double real_size = 46376.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		multiplier::ff ff(&m,30);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>(ff) << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(fateman2_test)
{
	std::cout << "Fateman 2:\n";
	std::cout << "==========\n\n";
	p_type x("x"), y("y"), z("z"), t("t");
	auto f = x + y + z + t + 1;
	auto tmp(f);
	for (auto i = 1; i < 30; ++i) {
		f *= tmp;
	}
	auto g = f + 1;
	const double real_size = 635376.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>() << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(fateman2_truncated_test)
{
	std::cout << "Fateman 2 truncated:\n";
	std::cout << "====================\n\n";
	p_type x("x"), y("y"), z("z"), t("t");
	auto f = x + y + z + t + 1;
	auto tmp(f);
	for (auto i = 1; i < 30; ++i) {
		f *= tmp;
	}
	auto g = f + 1;
	const double real_size = 46376.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		multiplier::ff ff(&m,30);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>(ff) << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(pearce1_test)
{
	std::cout << "Pearce 1:\n";
	std::cout << "=========\n\n";
	p_type x("x"), y("y"), z("z"), t("t"), u("u");
	auto f = (x + y + z*z*2 + t*t*t*3 + u*u*u*u*u*5 + 1);
	auto tmp_f(f);
	auto g = (u + t + z*z*2 + y*y*y*3 + x*x*x*x*x*5 + 1);
	auto tmp_g(g);
	for (int i = 1; i < 12; ++i) {
		f *= tmp_f;
		g *= tmp_g;
	}
	const double real_size = 5821335.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>() << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(pearce1_truncated_test)
{
	std::cout << "Pearce 1 truncated:\n";
	std::cout << "===================\n\n";
	p_type x("x"), y("y"), z("z"), t("t"), u("u");
	auto f = (x + y + z*z*2 + t*t*t*3 + u*u*u*u*u*5 + 1);
	auto tmp_f(f);
	auto g = (u + t + z*z*2 + y*y*y*3 + x*x*x*x*x*5 + 1);
	auto tmp_g(g);
	for (int i = 1; i < 12; ++i) {
		f *= tmp_f;
		g *= tmp_g;
	}
	const double real_size = 3419167.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		multiplier::ff ff(&m,60);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>(ff) << '\n';
		p_type::set_auto_truncate_degree(60);
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(pearce2_test)
{
	std::cout << "Pearce 2:\n";
	std::cout << "=========\n\n";
	p_type x("x"), y("y"), z("z"), t("t"), u("u");
	auto f = (x + y + z*z*2 + t*t*t*3 + u*u*u*u*u*5 + 1);
	auto tmp_f(f);
	auto g = (u + t + z*z*2 + y*y*y*3 + x*x*x*x*x*5 + 1);
	auto tmp_g(g);
	for (int i = 1; i < 16; ++i) {
		f *= tmp_f;
		g *= tmp_g;
	}
	const double real_size = 28398035.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>() << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(pearce2_truncated_test)
{
	std::cout << "Pearce 2 truncated:\n";
	std::cout << "===================\n\n";
	p_type x("x"), y("y"), z("z"), t("t"), u("u");
	auto f = (x + y + z*z*2 + t*t*t*3 + u*u*u*u*u*5 + 1);
	auto tmp_f(f);
	auto g = (u + t + z*z*2 + y*y*y*3 + x*x*x*x*x*5 + 1);
	auto tmp_g(g);
	for (int i = 1; i < 16; ++i) {
		f *= tmp_f;
		g *= tmp_g;
	}
	const double real_size = 17860117.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		multiplier::ff ff(&m,85);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>(ff) << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(gastineau2_test)
{
	std::cout << "Gastineau 2:\n";
	std::cout << "============\n\n";
	p_type x("x"), y("y"), z("z"), t("t"), u("u");
	auto f = (x + y + z*z*2 + t*t*t*3 + u*u*u*u*u*5 + 1);
	auto tmp_f(f);
	auto g = (u + t + z*z*2 + y*y*y*3 + x*x*x*x*x*5 + 1);
	auto tmp_g(g);
	for (int i = 1; i < 25; ++i) {
		f *= tmp_f;
		g *= tmp_g;
	}
	const double real_size = 312855140.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>() << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(gastineau3_test)
{
	std::cout << "Gastineau 3:\n";
	std::cout << "============\n\n";
	p_type u("u"), v("v"), w("w"), x("x"), y("y");
	auto f = (1 + u*u + v + w*w + x - y*y);
	auto g = (1 + u + v*v + w + x*x + y*y*y);
	auto tmp_f(f), tmp_g(g);
	for (int i = 1; i < 28; ++i) {
		f *= tmp_f;
		g *= tmp_g;
	}
	g += 1;
	const double real_size = 144049555.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>() << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(audi_test)
{
	std::cout << "AuDi:\n";
	std::cout << "=====\n\n";
	p_type x1{"x1"}, x2{"x2"}, x3{"x3"}, x4{"x4"}, x5{"x5"}, x6{"x6"}, x7{"x7"},
		x8{"x8"}, x9{"x9"}, x10{"x10"};
	auto f = math::pow(1+x1+x2+x3+x4+x5+x6+x7+x8+x9+x10,10);
	auto g = math::pow(1-x1-x2-x3-x4-x5-x6-x7-x8-x9-x10,10);
	const double real_size = 17978389.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>() << '\n';
	}
	std::cout << "\n\n";
}

BOOST_AUTO_TEST_CASE(audi_truncated_test)
{
	std::cout << "AuDi truncated:\n";
	std::cout << "===============\n\n";
	p_type x1{"x1"}, x2{"x2"}, x3{"x3"}, x4{"x4"}, x5{"x5"}, x6{"x6"}, x7{"x7"},
		x8{"x8"}, x9{"x9"}, x10{"x10"};
	auto f = math::pow(1+x1+x2+x3+x4+x5+x6+x7+x8+x9+x10,10);
	auto g = math::pow(1-x1-x2-x3-x4-x5-x6-x7-x8-x9-x10,10);
	const double real_size = 122464.;
	for (unsigned nt = 1u; nt <= max_nt(); ++nt) {
		settings::set_n_threads(nt);
		multiplier m(f,g);
		multiplier::ff ff(&m,10);
		std::cout << real_size / m.estimate_final_series_size<1u,multiplier::p_mult>(ff) << '\n';
	}
	std::cout << "\n\n";
}
