#ifndef ______DEBUG_H_XYZPDQ
#define ______DEBUG_H_XYZPDQ

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

#endif //______DEBUG_H_XYZPDQ

