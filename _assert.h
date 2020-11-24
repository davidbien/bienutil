#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _assert.h

#include <assert.h>
#include <string>
#include "syslogmgr.h"

// This file defines assertion and runtime check stuff.
// We allow assertion when ASSERTSENABLED is set non-zero. Normal in release binaries this is not set.
// Runtime checks are always enabled as the code is expect to *need* to run - i.e. the running
//  of code inside of a runtime check is not optional.

#ifdef Assert
#error _assert.h: Assert() macro is already defined.
#endif

#ifndef ASSERTSENABLED
#ifdef	NDEBUG
#define ASSERTSENABLED 0
#else
#define ASSERTSENABLED 1
#endif // !NDEBUG
#endif // !ASSERTSENABLED

#ifndef ABORTONASSERT
// By default we do not abort on assert. Abort generates a core dump so if you want to examine the state of things you would want to abort.
#define ABORTONASSERT 0
#endif // !ABORTONASSERT

#ifndef ABORTONVERIFY
// By default we do not abort on VERIFY. Abort generates a core dump so if you want to examine the state of things you would want to abort.
#define ABORTONVERIFY 0
#endif // !ABORTONVERIFY

// This failure mimicks the ANSI standard failure: Print a message (in our case to the syslog and potentially as well to the screen) and then flush the log file and abort() (if _fAbort).
// Need to define this later so we can use Assert() in various objects that we'd like to.
void 
AssertVerify_LogMessage(  bool _fAbort, bool _fAssert, const char * _szAssertVerify, const char * _szAssertion, const char * _szFile, 
                          unsigned int _nLine, const char * _szFunction, const char * _szMesg, ... );

#define Verify(expr)							          \
     ( static_cast <bool> (expr)						\
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONVERIFY, false, "Verify", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, 0 ) )
#define VerifySz(expr, sz...)							  \
     ( static_cast <bool> (expr)					  \
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONVERIFY, false, "Verify", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, sz ) )

#if !ASSERTSENABLED
#define Assert(expr)		(static_cast<void>(0))
#define AssertSz(expr, sz...)		(static_cast<void>(0))
#else // #if !ASSERTSENABLED
#define Assert(expr)							          \
     ( static_cast <bool> (expr)						\
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONASSERT, true, "Assert", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, 0 ) )
#define AssertSz(expr, sz...)							  \
     ( static_cast <bool> (expr)					  \
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONASSERT, true, "Assert", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, sz ) )
#endif // #if !ASSERTSENABLED

