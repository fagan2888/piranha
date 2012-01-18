INCLUDE(CheckCXXCompilerFlag)

# Configuration for GCC.
IF(CMAKE_COMPILER_IS_GNUCXX)
	INCLUDE(CheckTypeSize)
	MESSAGE(STATUS "GNU compiler detected.")
	CHECK_CXX_COMPILER_FLAG(-Wnon-virtual-dtor GNUCXX_VIRTUAL_DTOR)
	IF(GNUCXX_VIRTUAL_DTOR)
		MESSAGE(STATUS "Enabling '-Wnon-virtual-dtor' compiler flag.")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wnon-virtual-dtor")
	ENDIF(GNUCXX_VIRTUAL_DTOR)
	CHECK_CXX_COMPILER_FLAG(-std=c++11 GNUCXX_STD_CPP11)
	IF(GNUCXX_STD_CPP11)
		MESSAGE(STATUS "c++11 support detected.")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
	ELSE(GNUCXX_STD_CPP11)
		CHECK_CXX_COMPILER_FLAG(-std=c++0x GNUCXX_STD_CPP0X)
		IF(GNUCXX_STD_CPP0X)
			MESSAGE(STATUS "c++11 support detected.")
			SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
		ELSE(GNUCXX_STD_CPP0X)
			MESSAGE(FATAL_ERROR "c++0x support could not be detected.")
		ENDIF(GNUCXX_STD_CPP0X)
	ENDIF(GNUCXX_STD_CPP11)
	CHECK_CXX_COMPILER_FLAG(-fvisibility-inlines-hidden GNUCXX_VISIBILITY_INLINES_HIDDEN)
	CHECK_CXX_COMPILER_FLAG(-fvisibility=hidden GNUCXX_VISIBILITY_HIDDEN)
	IF(GNUCXX_VISIBILITY_INLINES_HIDDEN AND GNUCXX_VISIBILITY_HIDDEN AND NOT MINGW)
		MESSAGE(STATUS "Enabling GCC visibility support.")
		SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
	ENDIF(GNUCXX_VISIBILITY_INLINES_HIDDEN AND GNUCXX_VISIBILITY_HIDDEN AND NOT MINGW)
	# Add to the base flags extra warnings. Also, additional flags to turn off some GCC warnings that in practice clutter the compilation output.
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic-errors -Wdisabled-optimization")
	# Suggested for multithreaded code.
	ADD_DEFINITIONS(-D_REENTRANT)
	# When compiling with GCC, enable the GNU extensions - used for instance in thread affinity settings.
	ADD_DEFINITIONS(-D_GNU_SOURCE)
	CHECK_TYPE_SIZE("__int128_t" GCC___INT128_T)
	IF(GCC___INT128_T)
		SET(PIRANHA_HAVE_INT128 "#define PIRANHA_INT128_T __int128_t")
	ELSE(GCC___INT128_T)
		CHECK_TYPE_SIZE("__int128" GCC___INT128)
		IF(GCC___INT128)
			SET(PIRANHA_HAVE_INT128 "#define PIRANHA_INT128_T __int128")
		ENDIF(GCC___INT128)
	ENDIF(GCC___INT128_T)
ENDIF(CMAKE_COMPILER_IS_GNUCXX)
