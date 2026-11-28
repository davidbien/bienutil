#pragma once

//          Copyright David Lawrence Bien 1997 - 2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// bienutiltest.h
// What each bienutil test program includes: bienutil as a program must include it, and Google test.
// A test program is one source file: it includes this and defines g_strProgramName ( bienutil's logging needs it ).
// dbien
// 27NOV2026

#ifdef WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif //WIN32
#define DBG_NEW new

#include <string>
#include "_compat.h"
#include "_heapchk.h"

#include "jsonstrm.h"
#include "jsonobjs.h"
#include "_compat.inl"
#include "syslogmgr.inl"

#include "gtest/gtest.h"
