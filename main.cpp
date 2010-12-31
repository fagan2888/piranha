#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

#include <gmpxx.h>

#include "src/piranha.hpp"

using namespace piranha;

void *allocate_function(std::size_t alloc_size)
{
	std::cout << "allocate\n";
	return std::malloc(alloc_size);
}

void *reallocate_function(void *ptr, std::size_t, std::size_t new_size)
{
	std::cout << "reallocate\n";
	return std::realloc(ptr,new_size);
}

void free_function(void *ptr, size_t)
{
	std::cout << "free\n";
	std::free(ptr);
}

int main()
{
//	mp_set_memory_functions(allocate_function,reallocate_function,free_function);
	integer i(4), j(0);
	std::cout << (i / 3.) << '\n';
}
