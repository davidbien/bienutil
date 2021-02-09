#pragma once

// _heapchk.h
// Check the heap. Generally we only do this in debug but it can be turned on in retail.

#ifdef WIN32 // In Linux we link with -lmcheck to enable intermittent heap checking.
#ifndef HEAPCHECKENABLED
#ifdef	NDEBUG
#define HEAPCHECKENABLED 0
#else
#define HEAPCHECKENABLED 1
#endif // !NDEBUG
#endif //!HEAPCHECKENABLED
#if HEAPCHECKENABLED
#define CHECK_HEAP() _heapchk()
#else //!HEAPCHECKENABLED
#define CHECK_HEAP()
#endif //!HEAPCHECKENABLED
#else //!WIN32
#define CHECK_HEAP()
#endif //!WIN32
