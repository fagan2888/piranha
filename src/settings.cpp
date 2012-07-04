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

#include <stdexcept>
#include <utility>

#include "config.hpp"
#include "exceptions.hpp"
#include "runtime_info.hpp"
#include "settings.hpp"
#include "threading.hpp"

namespace piranha
{

// Static init.
mutex settings::m_mutex;
std::pair<bool,unsigned> settings::m_n_threads(false,0u);
std::pair<bool,unsigned> settings::m_cache_line_size(false,0u);
bool settings::m_tracing = false;
unsigned settings::m_max_char_output = settings::m_default_max_char_output;
bool settings::m_destruction_checks = true;

/// Get the number of threads available for use by piranha.
/**
 * The initial value is set to the maximum between 1 and piranha::runtime_info::get_hardware_concurrency().
 * 
 * @return the number of threads that will be available for use by piranha.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
unsigned settings::get_n_threads()
{
	lock_guard<mutex>::type lock(m_mutex);
	if (unlikely(!m_n_threads.first)) {
		const auto candidate = runtime_info::get_hardware_concurrency();
		m_n_threads.second = (candidate > 0u) ? candidate : 1u;
		m_n_threads.first = true;
	}
	return m_n_threads.second;
}

/// Set the number of threads available for use by piranha.
/**
 * @param[in] n the desired number of threads.
 * 
 * @throws std::invalid_argument if <tt>n == 0</tt>.
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::set_n_threads(unsigned n)
{
	if (n == 0u) {
		piranha_throw(std::invalid_argument,"the number of threads must be strictly positive");
	}
	lock_guard<mutex>::type lock(m_mutex);
	m_n_threads.first = true;
	m_n_threads.second = n;
}

/// Reset the number of threads available for use by piranha.
/**
 * Will set the number of threads to the maximum between 1 and piranha::runtime_info::get_hardware_concurrency().
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::reset_n_threads()
{
	lock_guard<mutex>::type lock(m_mutex);
	const auto candidate = runtime_info::get_hardware_concurrency();
	m_n_threads.second = (candidate > 0u) ? candidate : 1u;
	m_n_threads.first = true;
}

/// Get the cache line size.
/**
 * The initial value is set to the output of piranha::runtime_info::get_cache_line_size(). The value
 * can be overridden with set_cache_line_size() in case the detection fails and the value is set to zero.
 * 
 * @return data cache line size (in bytes).
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
unsigned settings::get_cache_line_size()
{
	lock_guard<mutex>::type lock(m_mutex);
	if (unlikely(!m_cache_line_size.first)) {
		m_cache_line_size.second = runtime_info::get_cache_line_size();
		m_cache_line_size.first = true;
	}
	return m_cache_line_size.second;
}

/// Set the cache line size.
/**
 * Overrides the detected cache line size. This method should be used only if the automatic
 * detection fails.
 * 
 * @param[in] n data cache line size (in bytes).
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::set_cache_line_size(unsigned n)
{
	lock_guard<mutex>::type lock(m_mutex);
	m_cache_line_size.first = true;
	m_cache_line_size.second = n;
}

/// Reset the cache line size.
/**
 * Will set the value to the output of piranha::runtime_info::get_cache_line_size().
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::reset_cache_line_size()
{
	lock_guard<mutex>::type lock(m_mutex);
	m_cache_line_size.second = runtime_info::get_cache_line_size();
	m_cache_line_size.first = true;
}

/// Get tracing status.
/**
 * Tracing is disabled by default on program startup.
 * 
 * @return \p true if tracing is enabled, \p false otherwise.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
bool settings::get_tracing()
{
	lock_guard<mutex>::type lock(m_mutex);
	return m_tracing;
}

/// Set tracing status.
/**
 * Tracing is disabled by default on program startup.
 * 
 * @param[in] flag \p true to enable tracing, \p false to disable it.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::set_tracing(bool flag)
{
	lock_guard<mutex>::type lock(m_mutex);
	m_tracing = flag;
}

/// Get max char output.
/**
 * @return the maximum number of character displayed when printing series.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
unsigned settings::get_max_char_output()
{
	lock_guard<mutex>::type lock(m_mutex);
	return m_max_char_output;
}

/// Set max char output.
/**
 * @param[in] n the maximum number of character to be displayed when printing series.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::set_max_char_output(unsigned n)
{
	lock_guard<mutex>::type lock(m_mutex);
	m_max_char_output = n;
}

/// Reset max char output.
/**
 * Will set the max char output value to the default.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::reset_max_char_output()
{
	lock_guard<mutex>::type lock(m_mutex);
	m_max_char_output = m_default_max_char_output;
}

/// Get status of destruction checks.
/**
 * In debug mode, consistency checks on destruction are run in some of piranha's classes.
 * Since some of these checks rely on the presence of static global variables, if the class instances
 * from which the checks are called have themselves static duration, on program exit the undefined
 * order of destruction of static objects could lead to invalid memory accesses. This could happen for instance
 * if custom partial derivative functors are registered from piranha::series that maintain internally a copy
 * of a series.
 * 
 * For this reason, a boolean flag is kept within the settings class to enable/disable such destruction checks
 * at runtime. This method allows to query the status of such flag, which is set to \p true on program startup.
 * 
 * If the program is not compiled in debug mode, this flag has no effect.
 * 
 * @return status flag of destruction checks.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
bool settings::get_destruction_checks()
{
	lock_guard<mutex>::type lock(m_mutex);
	return m_destruction_checks;
}

/// Set the status flag for destruction checks.
/**
 * This method is used to enable/disable the debug consistency checks on destruction for some of piranha's classes.
 * See the explanation in settings::get_destruction_checks().
 * 
 * @param[in] flag desired value for the flag.
 * 
 * @throws std::system_error in case of failure(s) by threading primitives.
 */
void settings::set_destruction_checks(bool flag)
{
	lock_guard<mutex>::type lock(m_mutex);
	m_destruction_checks = flag;
}

}
