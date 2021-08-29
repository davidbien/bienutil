#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _debug.h

// This file defines debugging stuff.

#ifdef NDEBUG
#define __DEBUG_STMT( s )
#else //NDEBUG
#define __DEBUG_STMT( s ) s;
#endif //NDEBUG

#ifdef NDEBUG
#define __DEBUG_FRAG( s )
#else //NDEBUG
#define __DEBUG_FRAG( s ) s
#endif //NDEBUG

#ifdef NDEBUG
#define __DEBUG_COMMA( s )
#else //NDEBUG
#define __DEBUG_COMMA( s )  ,s
#endif //NDEBUG

#ifdef NDEBUG
#define __DEBUG_COMMA_2( s1, s2 )
#else //NDEBUG
#define __DEBUG_COMMA_2( s1, s2 ) s1##,##s2
#endif //NDEBUG

#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#elif defined( __clang__ )
#if __has_builtin(__builtin_debugtrap)
#define DEBUG_BREAK __builtin_debugtrap()
#else
#define DEBUG_BREAK __builtin_trap()
#endif
#elif defined( __GNUC__ )
#define DEBUG_BREAK __builtin_trap()
#else
#error Need to know the OS/compiler for breaking into the debugger.
#endif


