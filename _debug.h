#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _debug.h

// This file defines debugging stuff.

// Sometimes you want to run the server (for instance) in a debugger and not debug break due to assertions.
//#define DEBUG_NEVERDEBUGBREAK

// Choose between debug and retail token based on the NDEBUG macro.
#ifdef NDEBUG
#define DEBUG_OR_RETAIL(debug_value, retail_value) (retail_value)
#else
#define DEBUG_OR_RETAIL(debug_value, retail_value) (debug_value)
#endif

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

#ifdef DEBUG_NEVERDEBUGBREAK
#define DEBUG_BREAK
#else //DEBUG_NEVERDEBUGBREAK
#ifdef _MSC_VER
#define DEBUG_BREAK __debugbreak()
#elif defined( __clang__ )
#ifdef __ANDROID__
#define DEBUG_BREAK raise(SIGTRAP)
#else // !__ANDROID__
#if __has_builtin(__builtin_debugtrap)
#define DEBUG_BREAK __builtin_debugtrap()
#else
#define DEBUG_BREAK __builtin_trap()
#endif
#endif // !__ANDROID__
#elif defined( __GNUC__ )
#define DEBUG_BREAK __builtin_trap()
#else
#error Need to know the OS/compiler for breaking into the debugger.
#endif
#endif //DEBUG_NEVERDEBUGBREAK


